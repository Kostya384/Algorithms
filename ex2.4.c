#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

/* ---------- типы ---------- */

typedef struct {
    char *name;
    long long size;
    long long importance;
} FileDesc;

typedef struct {
    FileDesc *files;     /* выбранные файлы (копии), в порядке входных данных */
    size_t count;
    long long total_size;
    long long total_importance;
} BackupPlan;

typedef enum {
    OK = 0,
    ERR_ARGS,
    ERR_READ,
    ERR_ALLOC,
    ERR_WRITE
} Status;

/* ---------- валидация ---------- */

static int parse_ll(const char *s, long long *out) {
    if (!s || !*s) return 0;
    errno = 0;
    char *end = NULL;
    long long v = strtoll(s, &end, 10);
    if (errno || *end) return 0;
    *out = v;
    return 1;
}

/* ---------- освобождение ---------- */

static void free_files(FileDesc *arr, size_t n) {
    if (!arr) return;
    for (size_t i = 0; i < n; ++i) free(arr[i].name);
    free(arr);
}

static void free_plan(BackupPlan *p) {
    if (!p) return;
    if (p->files) {
        for (size_t i = 0; i < p->count; ++i) free(p->files[i].name);
        free(p->files);
    }
    p->files = NULL;
    p->count = 0;
    p->total_size = 0;
    p->total_importance = 0;
}

/* ---------- выбор файлов (0/1 рюкзак) ---------- */

/*
 * files  — массив описаний
 * n      — количество
 * cap    — вместимость
 * plan   — результат (владелец — вызывающий код; освобождать через free_plan)
 */
static Status choose_files(const FileDesc *files, size_t n, long long cap,
                           BackupPlan *plan) {
    if (!plan) return ERR_ARGS;
    plan->files = NULL;
    plan->count = 0;
    plan->total_size = 0;
    plan->total_importance = 0;

    if (n == 0 || cap <= 0) return OK;

    /* вместимость в size_t, защита от переполнения */
    if ((unsigned long long)cap > (unsigned long long)SIZE_MAX - 1)
        return ERR_ALLOC;
    size_t W = (size_t)cap + 1;

    /* общий размер матрицы parent: n * W */
    if (n > SIZE_MAX / W) return ERR_ALLOC;
    size_t cells = n * W;

    /* dp[w] — max importance для вместимости w */
    long long *dp = malloc(W * sizeof(*dp));
    if (!dp) return ERR_ALLOC;
    for (size_t w = 0; w < W; ++w) dp[w] = 0;

    /* parent[i*W + w] = 1, если на шаге i (файл i) взяли при вместимости w */
    unsigned char *parent = calloc(cells, 1);
    if (!parent) { free(dp); return ERR_ALLOC; }

    for (size_t i = 0; i < n; ++i) {
        long long sz = files[i].size;
        long long im = files[i].importance;
        if (sz < 0 || im < 0) { free(dp); free(parent); return ERR_ARGS; }

        /* обход с конца, чтобы не взять файл дважды */
        for (size_t w = W; w-- > 0; ) {
            if ((long long)w >= sz) {
                size_t prev = w - (size_t)sz;
                long long cand = dp[prev] + im;
                if (cand > dp[w]) {
                    dp[w] = cand;
                    parent[i * W + w] = 1;
                }
            }
        }
    }

    long long best_imp = dp[W - 1];

    if (best_imp == 0) {
        /* пустой набор */
        free(dp); free(parent);
        return OK;
    }

    /* восстановление набора: идём с конца, при parent[i][w]==1 — берём i */
    unsigned char *taken = calloc(n, 1);
    if (!taken) { free(dp); free(parent); return ERR_ALLOC; }

    size_t w = W - 1;
    for (size_t i = n; i-- > 0; ) {
        if (parent[i * W + w]) {
            taken[i] = 1;
            w -= (size_t)files[i].size;
        }
    }

    /* подсчёт */
    size_t cnt = 0;
    long long tsz = 0, timp = 0;
    for (size_t i = 0; i < n; ++i) {
        if (taken[i]) { ++cnt; tsz += files[i].size; timp += files[i].importance; }
    }

    /* копии выбранных файлов — в порядке входных данных */
    FileDesc *sel = malloc(cnt * sizeof(*sel));
    if (!sel) { free(taken); free(dp); free(parent); return ERR_ALLOC; }

    size_t k = 0;
    Status st = OK;
    for (size_t i = 0; i < n && st == OK; ++i) {
        if (!taken[i]) continue;
        sel[k].size = files[i].size;
        sel[k].importance = files[i].importance;
        sel[k].name = strdup(files[i].name);
        if (!sel[k].name) st = ERR_ALLOC;
        else ++k;
    }

    free(taken); free(dp); free(parent);

    if (st != OK) {
        for (size_t j = 0; j < k; ++j) free(sel[j].name);
        free(sel);
        return st;
    }

    plan->files = sel;
    plan->count = cnt;
    plan->total_size = tsz;
    plan->total_importance = timp;
    return OK;
}

/* ---------- ввод ---------- */

/* читает строку произвольной длины; 1 — ok, 0 — EOF, -1 — ошибка/память */
static int read_line(FILE *f, char **out) {
    size_t cap = 128, len = 0;
    char *buf = malloc(cap);
    if (!buf) return -1;
    int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            size_t nc = cap * 2;
            char *nb = realloc(buf, nc);
            if (!nb) { free(buf); return -1; }
            buf = nb; cap = nc;
        }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) { free(buf); return 0; }
    if (len && buf[len - 1] == '\r') --len;
    buf[len] = '\0';
    *out = buf;
    return 1;
}

/* разбивает строку на ровно 3 токена (name size importance) */
static Status parse_file_line(const char *line, FileDesc *fd) {
    char *copy = strdup(line);
    if (!copy) return ERR_ALLOC;

    char *tok[3] = {0};
    int t = 0;
    char *p = copy;
    while (*p && t < 3) {
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) break;
        tok[t++] = p;
        while (*p && *p != ' ' && *p != '\t') ++p;
        if (*p) *p++ = '\0';
    }
    int extra = 0;
    while (*p) { if (*p != ' ' && *p != '\t') { extra = 1; break; } ++p; }

    Status st = OK;
    fd->name = NULL;
    if (t != 3 || extra) st = ERR_READ;
    else if (!tok[0][0]) st = ERR_READ;
    else if (!parse_ll(tok[1], &fd->size) || fd->size <= 0) st = ERR_READ;
    else if (!parse_ll(tok[2], &fd->importance) || fd->importance < 0) st = ERR_READ;
    else {
        fd->name = strdup(tok[0]);
        if (!fd->name) st = ERR_ALLOC;
    }
    free(copy);
    return st;
}

static Status read_input(FileDesc **out, size_t *out_n, long long *out_cap) {
    char *line = NULL;
    int r = read_line(stdin, &line);
    if (r <= 0) return ERR_READ;

    /* первая строка: n capacity */
    char *tok[2] = {0};
    int t = 0;
    char *p = line;
    while (*p && t < 2) {
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) break;
        tok[t++] = p;
        while (*p && *p != ' ' && *p != '\t') ++p;
        if (*p) *p++ = '\0';
    }
    int extra = 0;
    while (*p) { if (*p != ' ' && *p != '\t') { extra = 1; break; } ++p; }

    long long nn = 0, cap = 0;
    Status st = OK;
    if (t != 2 || extra) st = ERR_READ;
    else if (!parse_ll(tok[0], &nn) || nn <= 0) st = ERR_READ;
    else if (!parse_ll(tok[1], &cap) || cap < 0) st = ERR_READ;
    free(line);
    if (st != OK) return st;

    if ((unsigned long long)nn > SIZE_MAX / sizeof(FileDesc)) return ERR_ALLOC;
    FileDesc *arr = malloc((size_t)nn * sizeof(*arr));
    if (!arr) return ERR_ALLOC;

    size_t i = 0;
    for (; i < (size_t)nn; ) {
        char *ln = NULL;
        r = read_line(stdin, &ln);
        if (r <= 0) { free(ln); st = ERR_READ; break; }
        if (!ln[0]) { free(ln); continue; }
        FileDesc fd;
        st = parse_file_line(ln, &fd);
        free(ln);
        if (st != OK) break;
        arr[i++] = fd;
    }

    if (st != OK) { free_files(arr, i); return st; }
    *out = arr; *out_n = (size_t)nn; *out_cap = cap;
    return OK;
}

/* ---------- вывод ---------- */

static Status print_plan(const BackupPlan *p) {
    if (printf("TOTAL_SIZE %lld\nTOTAL_IMPORTANCE %lld\nFILES %zu\n",
               p->total_size, p->total_importance, p->count) < 0)
        return ERR_WRITE;
    for (size_t i = 0; i < p->count; ++i)
        if (printf("%s\n", p->files[i].name) < 0) return ERR_WRITE;
    return OK;
}

/* ---------- main ---------- */

int main(void) {
    FileDesc *files = NULL;
    size_t n = 0;
    long long cap = 0;

    Status st = read_input(&files, &n, &cap);
    if (st != OK) {
        switch (st) {
            case ERR_READ:  fprintf(stderr, "Bad input\n"); break;
            case ERR_ALLOC: fprintf(stderr, "Alloc failed\n"); break;
            default:        fprintf(stderr, "Error\n");
        }
        return st;
    }

    BackupPlan plan = {0};
    st = choose_files(files, n, cap, &plan);
    free_files(files, n);

    if (st != OK) {
        fprintf(stderr, st == ERR_ALLOC ? "Alloc failed\n" : "Error\n");
        return st;
    }

    st = print_plan(&plan);
    free_plan(&plan);
    if (st != OK) fprintf(stderr, "Write error\n");
    return st;
}