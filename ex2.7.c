#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>

/* ---------------- типы ---------------- */

typedef struct {
    double *coords;
    size_t n;
    int id;
} Vector;

typedef struct {
    const double *data;  /* n*n, плоский, data[i*n + j] */
    size_t n;
} Matrix;

/* универсальный указатель на функцию нормы */
typedef double (*NormFunc)(const double *v, size_t n, const void *ctx);

typedef struct {
    double *norm_values;   /* [norm_count] макс. норма по каждой */
    size_t *counts;        /* [norm_count] сколько векторов достигли максимума */
    size_t **indices;      /* [norm_count] массивы индексов (в порядке входа) */
    size_t norm_count;
} LongestResult;

typedef enum {
    OK = 0,
    ERR_ARGS,
    ERR_DIM,
    ERR_NORM_CTX,
    ERR_ALLOC,
    ERR_DOMAIN
} Status;

/* ---------------- конкретные нормы ---------------- */

/* ∞-норма: max_j |x_j| */
static double norm_inf(const double *v, size_t n, const void *ctx) {
    (void)ctx;
    double m = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double a = fabs(v[i]);
        if (a > m) m = a;
    }
    return m;
}

/* p-норма: (Σ|x_j|^p)^(1/p), p >= 1; ctx -> const double* (p) */
static double norm_p(const double *v, size_t n, const void *ctx) {
    const double *pp = (const double *)ctx;
    double p = *pp;
    double s = 0.0;
    for (size_t i = 0; i < n; ++i)
        s += pow(fabs(v[i]), p);
    return pow(s, 1.0 / p);
}

/* A-норма: sqrt((Ax, x)); ctx -> const Matrix* */
static double norm_A(const double *v, size_t n, const void *ctx) {
    const Matrix *A = (const Matrix *)ctx;
    /* (Ax)_i = Σ_j A[i*n+j] * x_j */
    double ax_dot_x = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double ax = 0.0;
        for (size_t j = 0; j < n; ++j)
            ax += A->data[i * n + j] * v[j];
        ax_dot_x += ax * v[i];
    }
    if (ax_dot_x < 0.0) ax_dot_x = 0.0; /* погрешность округления */
    return sqrt(ax_dot_x);
}

/* ---------------- валидация контекста нормы ---------------- */

/* Проверить, что контекст нормы согласован (n, p, матрица) — до вычислений */
static Status validate_norm(NormFunc f, const void *ctx, size_t n) {
    if (!f) return ERR_ARGS;
    if (f == norm_p) {
        if (!ctx) return ERR_NORM_CTX;
        const double *p = (const double *)ctx;
        if (*p < 1.0 || !isfinite(*p)) return ERR_NORM_CTX;
    } else if (f == norm_A) {
        if (!ctx) return ERR_NORM_CTX;
        const Matrix *A = (const Matrix *)ctx;
        if (!A->data || A->n != n) return ERR_NORM_CTX;
    }
    /* для остальных (∞, пользовательские) — контекст не проверяем */
    return OK;
}

/* ---------------- освобождение результата ---------------- */

static void free_result(LongestResult *r) {
    if (!r) return;
    if (r->indices) {
        for (size_t k = 0; k < r->norm_count; ++k) free(r->indices[k]);
        free(r->indices);
    }
    free(r->norm_values);
    free(r->counts);
    r->indices = NULL;
    r->norm_values = NULL;
    r->counts = NULL;
    r->norm_count = 0;
}

/* ---------------- основная функция ---------------- */

/*
 * Параметры:
 *   n          — размерность пространства
 *   vec_count  — количество переданных векторов (в ...)
 *   norm_count — количество переданных норм (в ...)
 *   eps        — эпсилон для сравнения норм
 *   out        — результат
 *   ...        — vec_count экземпляров Vector, затем norm_count пар (NormFunc, const void*)
 */
static Status find_longest_vectors(size_t n,
                                   size_t vec_count,
                                   size_t norm_count,
                                   double eps,
                                   LongestResult *out,
                                   ...) {
    if (!out) return ERR_ARGS;
    out->norm_values = NULL;
    out->counts = NULL;
    out->indices = NULL;
    out->norm_count = 0;

    if (n == 0 || norm_count == 0 || eps < 0.0) return ERR_ARGS;

    Status st = OK;

    /* --- читаем векторы в массив --- */
    if (vec_count > SIZE_MAX / sizeof(Vector)) return ERR_ALLOC;
    Vector *vecs = NULL;
    if (vec_count > 0) {
        vecs = malloc(vec_count * sizeof(Vector));
        if (!vecs) return ERR_ALLOC;
    }

    va_list ap;
    va_start(ap, out);

    for (size_t i = 0; i < vec_count; ++i) {
        vecs[i] = va_arg(ap, Vector);
        if (!vecs[i].coords || vecs[i].n != n) { st = ERR_DIM; break; }
    }

    /* --- читаем нормы --- */
    NormFunc *funcs = NULL;
    const void **ctxs = NULL;
    if (st == OK) {
        funcs = malloc(norm_count * sizeof(*funcs));
        ctxs  = malloc(norm_count * sizeof(*ctxs));
        if (!funcs || !ctxs) st = ERR_ALLOC;
    }

    if (st == OK) {
        for (size_t k = 0; k < norm_count; ++k) {
            funcs[k] = va_arg(ap, NormFunc);
            ctxs[k]  = va_arg(ap, const void *);
        }
    }
    va_end(ap);

    if (st != OK) {
        free(vecs); free(funcs); free(ctxs);
        return st;
    }

    /* --- валидация норм --- */
    for (size_t k = 0; k < norm_count && st == OK; ++k)
        st = validate_norm(funcs[k], ctxs[k], n);

    if (st != OK) {
        free(vecs); free(funcs); free(ctxs);
        return st;
    }

    /* --- выделяем результат --- */
    out->norm_values = malloc(norm_count * sizeof(double));
    out->counts      = malloc(norm_count * sizeof(size_t));
    out->indices     = malloc(norm_count * sizeof(size_t *));
    if (!out->norm_values || !out->counts || !out->indices) {
        free(vecs); free(funcs); free(ctxs);
        free_result(out);
        return ERR_ALLOC;
    }
    for (size_t k = 0; k < norm_count; ++k) {
        out->indices[k] = NULL;
        out->counts[k] = 0;
        out->norm_values[k] = 0.0;
    }
    out->norm_count = norm_count;

    /* --- для каждой нормы ищем максимум и все векторы, его достигающие --- */
    for (size_t k = 0; k < norm_count && st == OK; ++k) {
        if (vec_count == 0) {
            /* нет векторов — пустой результат для этой нормы */
            out->norm_values[k] = 0.0;
            out->counts[k] = 0;
            out->indices[k] = NULL;
            continue;
        }

        /* вычисляем нормы всех векторов */
        double *vals = malloc(vec_count * sizeof(double));
        if (!vals) { st = ERR_ALLOC; break; }

        for (size_t i = 0; i < vec_count; ++i) {
            vals[i] = funcs[k](vecs[i].coords, n, ctxs[k]);
            if (!isfinite(vals[i])) { st = ERR_DOMAIN; break; }
        }
        if (st != OK) { free(vals); break; }

        /* максимум */
        double mx = vals[0];
        for (size_t i = 1; i < vec_count; ++i)
            if (vals[i] > mx) mx = vals[i];

        /* количество тех, кто «достигает максимума» с точностью eps */
        size_t cnt = 0;
        for (size_t i = 0; i < vec_count; ++i)
            if (fabs(vals[i] - mx) <= eps) ++cnt;

        size_t *idxs = malloc(cnt * sizeof(size_t));
        if (!idxs) { free(vals); st = ERR_ALLOC; break; }

        size_t t = 0;
        for (size_t i = 0; i < vec_count; ++i)
            if (fabs(vals[i] - mx) <= eps) idxs[t++] = i;

        free(vals);
        out->norm_values[k] = mx;
        out->counts[k] = cnt;
        out->indices[k] = idxs;
    }

    free(vecs); free(funcs); free(ctxs);

    if (st != OK) { free_result(out); return st; }
    return OK;
}

/* ---------------- демонстрация ---------------- */

int main(void) {
    /* n = 3, три вектора */
    double v1c[3] = { 3.0,  4.0,  0.0};  /* |v1|_2 = 5,  |v1|_inf = 4 */
    double v2c[3] = {-4.0,  3.0,  0.0};  /* |v2|_2 = 5,  |v2|_inf = 4 */
    double v3c[3] = { 1.0,  1.0,  1.0};  /* |v3|_2 = sqrt(3) ≈ 1.73, |v3|_inf = 1 */

    Vector v1 = { v1c, 3, 1 };
    Vector v2 = { v2c, 3, 2 };
    Vector v3 = { v3c, 3, 3 };

    /* A — единичная матрица 3x3, тогда ‖x‖_A = ‖x‖_2 */
    double Aid[9] = {1,0,0, 0,1,0, 0,0,1};
    Matrix A = { Aid, 3 };

    /* p-норма с p = 3 */
    double p3 = 3.0;

    const double eps = 1e-9;

    LongestResult r;
    Status st = find_longest_vectors(3,
                                     3,   /* векторов */
                                     3,   /* норм */
                                     eps,
                                     &r,
                                     v1, v2, v3,
                                     norm_inf, (const void *)NULL,
                                     norm_p,   (const void *)&p3,
                                     norm_A,   (const void *)&A);

    if (st != OK) {
        fprintf(stderr, "Error: %d\n", st);
        return st;
    }

    const char *names[3] = { "inf", "p=3", "A" };
    for (size_t k = 0; k < r.norm_count; ++k) {
        printf("Norm %-4s max = %.6f, count = %zu, ids =",
               names[k], r.norm_values[k], r.counts[k]);
        for (size_t i = 0; i < r.counts[k]; ++i) {
            size_t vi = r.indices[k][i];
            int id = (vi == 0) ? v1.id : (vi == 1) ? v2.id : v3.id;
            printf(" %d", id);
        }
        printf("\n");
    }

    free_result(&r);
    return 0;
}