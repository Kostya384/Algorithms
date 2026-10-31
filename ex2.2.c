#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define EPS 1e-9

typedef struct {
    long long id;
    char *name;
    char *surname;
    double salary;
} Employee;

typedef enum {
    OK = 0,
    ERR_ARGS,
    ERR_FLAG,
    ERR_OPEN_IN,
    ERR_OPEN_OUT,
    ERR_READ,
    ERR_ALLOC,
    ERR_WRITE
} Status;

/* ---------- валидация ---------- */

static int is_alpha_str(const char *s) {
    if (!s || !*s) return 0;
    for (const char *p = s; *p; ++p)
        if (!isalpha((unsigned char)*p)) return 0;
    return 1;
}

static int parse_ull(const char *s, long long *out) {
    if (!s || !*s) return 0;
    errno = 0;
    char *end = NULL;
    long long v = strtoll(s, &end, 10);
    if (errno || *end || v < 0) return 0;
    *out = v;
    return 1;
}

static int parse_salary(const char *s, double *out) {
    if (!s || !*s) return 0;
    errno = 0;
    char *end = NULL;
    double v = strtod(s, &end);
    if (errno || *end || v < 0) return 0;
    *out = v;
    return 1;
}

/* ---------- чтение строки произвольной длины ---------- */
/* Возвращает 1 — прочитана строка (возможно пустая), 0 — EOF, -1 — ошибка/нет памяти */
static int read_line(FILE *f, char **out) {
    size_t cap = 64, len = 0;
    char *buf = malloc(cap);
    if (!buf) return -1;
    int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            size_t ncap = cap * 2;
            char *nb = realloc(buf, ncap);
            if (!nb) { free(buf); return -1; }
            buf = nb; cap = ncap;
        }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) { free(buf); return 0; }
    buf[len] = '\0';
    *out = buf;
    return 1;
}

/* ---------- разбор строки в Employee ---------- */
/* Формат: id name surname salary, разделители — пробелы/табы */
static Status parse_employee(const char *line, Employee *e) {
    char *copy = strdup(line);
    if (!copy) return ERR_ALLOC;

    char *tokens[4] = {0};
    int n = 0;
    char *p = copy;
    while (*p && n < 4) {
        while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
        if (!*p) break;
        tokens[n++] = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r') ++p;
        if (*p) *p++ = '\0';
    }
    // допускаем только 4 токена — иначе лишние данные 
    int extra = 0;
    while (*p) { if (*p != ' ' && *p != '\t' && *p != '\r') { extra = 1; break; } ++p; }

    Status st = OK;
    if (n != 4 || extra) st = ERR_READ;
    else if (!parse_ull(tokens[0], &e->id)) st = ERR_READ;
    else if (!is_alpha_str(tokens[1])) st = ERR_READ;
    else if (!is_alpha_str(tokens[2])) st = ERR_READ;
    else if (!parse_salary(tokens[3], &e->salary)) st = ERR_READ;

    if (st == OK) {
        e->name = strdup(tokens[1]);
        e->surname = strdup(tokens[2]);
        if (!e->name || !e->surname) st = ERR_ALLOC;
    }
    free(copy);
    return st;
}

/* ---------- освобождение ---------- */
static void free_employees(Employee *arr, size_t n) {
    if (!arr) return;
    for (size_t i = 0; i < n; ++i) {
        free(arr[i].name);
        free(arr[i].surname);
    }
    free(arr);
}

/* ---------- загрузка файла ---------- */
static Status load_file(const char *path, Employee **out, size_t *out_n) {
    FILE *f = fopen(path, "r");
    if (!f) return ERR_OPEN_IN;

    Employee *arr = NULL;
    size_t n = 0, cap = 0;
    Status st = OK;

    for (;;) {
        char *line = NULL;
        int r = read_line(f, &line);
        if (r == 0) break;
        if (r < 0) { st = ERR_ALLOC; break; }
        if (line[0] == '\0') { free(line); continue; }

        Employee e = {0};
        st = parse_employee(line, &e);
        free(line);
        if (st != OK) break;

        if (n == cap) {
            size_t ncap = cap ? cap * 2 : 8;
            Employee *na = realloc(arr, ncap * sizeof(*na));
            if (!na) { free(e.name); free(e.surname); st = ERR_ALLOC; break; }
            arr = na; cap = ncap;
        }
        arr[n++] = e;
    }
    fclose(f);

    if (st != OK) { free_employees(arr, n); return st; }
    *out = arr;
    *out_n = n;
    return OK;
}

/* ---------- компаратор qsort ---------- */
static int cmp_asc(const void *a, const void *b) {
    const Employee *x = a, *y = b;
    double d = x->salary - y->salary;
    if (d > EPS) return 1;
    if (d < -EPS) return -1;
    int c = strcmp(x->surname, y->surname);
    if (c) return c;
    c = strcmp(x->name, y->name);
    if (c) return c;
    if (x->id < y->id) return -1;
    if (x->id > y->id) return 1;
    return 0;
}

static int cmp_desc(const void *a, const void *b) {
    return -cmp_asc(a, b);
}

/* ---------- запись ---------- */
static Status write_file(const char *path, const Employee *arr, size_t n) {
    FILE *f = fopen(path, "w");
    if (!f) return ERR_OPEN_OUT;
    for (size_t i = 0; i < n; ++i) {
        if (fprintf(f, "%lld %s %s %.10g\n",
                    arr[i].id, arr[i].name, arr[i].surname, arr[i].salary) < 0) {
            fclose(f);
            return ERR_WRITE;
        }
    }
    if (fclose(f) != 0) return ERR_WRITE;
    return OK;
}

/* ---------- main ---------- */
int main(int argc, char **argv) {
    Status st = OK;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <in> <-a|-d|/a|/d> <out>\n", argv[0]);
        return ERR_ARGS;
    }

    const char *in_path = argv[1];
    const char *flag = argv[2];
    const char *out_path = argv[3];

    int asc;
    if ((flag[0] == '-' || flag[0] == '/') && flag[1] && !flag[2]) {
        if (flag[1] == 'a') asc = 1;
        else if (flag[1] == 'd') asc = 0;
        else { fprintf(stderr, "Bad flag\n"); return ERR_FLAG; }
    } else {
        fprintf(stderr, "Bad flag\n");
        return ERR_FLAG;
    }

    Employee *arr = NULL;
    size_t n = 0;
    st = load_file(in_path, &arr, &n);
    if (st != OK) {
        switch (st) {
            case ERR_OPEN_IN: fprintf(stderr, "Cannot open input\n"); break;
            case ERR_READ:    fprintf(stderr, "Bad record in input\n"); break;
            case ERR_ALLOC:   fprintf(stderr, "Alloc failed\n"); break;
            default:          fprintf(stderr, "Error\n");
        }
        return st;
    }

    if (n > 1)
        qsort(arr, n, sizeof(*arr), asc ? cmp_asc : cmp_desc);

    st = write_file(out_path, arr, n);
    free_employees(arr, n);

    if (st != OK) {
        fprintf(stderr, st == ERR_OPEN_OUT ? "Cannot open output\n" : "Write error\n");
    }
    return st;
}