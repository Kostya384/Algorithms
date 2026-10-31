#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

typedef enum {
    OK = 0,
    ERR_BASE,
    ERR_ARG,
    ERR_DIGIT,
    ERR_ALLOC
} Status;

/* ---------- цифра <-> значение ---------- */
static int digit_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return -1;
}

static char val_digit(int v) {
    return (v < 10) ? (char)('0' + v) : (char)('a' + v - 10);
}

/* ---------- валидация строки-числа в базе base ---------- */
/* Разрешён знак '+'? По условию числа неотрицательные — знак не нужен.
   Ведущие нули допускаются на входе (это же «представление»); на выходе не будет.
   Пустая строка — ошибка. Все символы должны быть цифрами base. */
static Status validate_num(const char *s, int base) {
    if (!s || !*s) return ERR_ARG;
    for (const char *p = s; *p; ++p) {
        int d = digit_val(*p);
        if (d < 0 || d >= base) return ERR_DIGIT;
    }
    return OK;
}

/* ---------- сложение двух чисел-строк в базе base ----------
   Возвращает через *out новую строку (malloc'ed, без ведущих нулей) — владелец вызывающий.
   Возврат — Status. */
static Status add_in_base(const char *a, const char *b, int base, char **out) {
    if (!out) return ERR_ARG;
    *out = NULL;
    if (base < 2 || base > 36) return ERR_BASE;

    Status st = validate_num(a, base);
    if (st != OK) return st;
    st = validate_num(b, base);
    if (st != OK) return st;

    size_t la = strlen(a), lb = strlen(b);
    size_t maxlen = (la > lb ? la : lb);
    /* результат не длиннее maxlen+1 (перенос) + '\0' */
    char *res = malloc(maxlen + 2);
    if (!res) return ERR_ALLOC;

    size_t ia = la, ib = lb, ir = 0;
    int carry = 0;
    while (ia > 0 || ib > 0 || carry) {
        int da = ia ? digit_val(a[--ia]) : 0;
        int db = ib ? digit_val(b[--ib]) : 0;
        int s = da + db + carry;
        res[ir++] = val_digit(s % base);
        carry = s / base;
    }
    res[ir] = '\0';

    /* перевернуть (строили с конца) */
    for (size_t i = 0, j = ir; i + 1 < j; ++i, --j) {
        char t = res[i]; res[i] = res[j - 1]; res[j - 1] = t;
    }

    /* убрать ведущие нули (но оставить хотя бы один '0') */
    size_t lead = 0;
    while (res[lead] == '0' && res[lead + 1] != '\0') ++lead;
    if (lead) memmove(res, res + lead, ir - lead + 1);

    *out = res;
    return OK;
}

/* ---------- основная функция: сумма count чисел в базе base ---------- */
/* Возвращает результат через *out (malloc'ed, без ведущих нулей). */
static Status sum_in_base(int base, size_t count, char **out, ...) {
    if (!out) return ERR_ARG;
    *out = NULL;
    if (base < 2 || base > 36) return ERR_BASE;

    char *acc = NULL;
    if (count == 0) {
        acc = strdup("0");
        if (!acc) return ERR_ALLOC;
        *out = acc;
        return OK;
    }

    va_list ap;
    va_start(ap, out);
    Status st = OK;
    for (size_t i = 0; i < count; ++i) {
        const char *s = va_arg(ap, const char *);
        if (i == 0) {
            st = validate_num(s, base);
            if (st != OK) break;
            acc = strdup(s);
            if (!acc) { st = ERR_ALLOC; break; }
        } else {
            char *tmp = NULL;
            st = add_in_base(acc, s, base, &tmp);
            free(acc);
            acc = tmp;
            if (st != OK) break;
        }
    }
    va_end(ap);

    if (st != OK) { free(acc); return st; }
    *out = acc;
    return OK;
}

/* ---------- main: демонстрация ---------- */
int main(void) {
    /* Демонстрация 1: base 10, 3 слагаемых */
    {
        char *res = NULL;
        Status st = sum_in_base(10, 3, &res,
                                "999999999999999999999999",
                                "1",
                                "1000000000000000000000000");
        if (st == OK) { printf("sum base 10 = %s\n", res); free(res); }
        else fprintf(stderr, "Error %d\n", st);
    }

    /* Демонстрация 2: base 2, 4 слагаемых */
    {
        char *res = NULL;
        Status st = sum_in_base(2, 4, &res, "1010", "1111", "1", "0");
        if (st == OK) { printf("sum base 2  = %s\n", res); free(res); }
        else fprintf(stderr, "Error %d\n", st);
    }

    /* Демонстрация 3: base 36 */
    {
        char *res = NULL;
        Status st = sum_in_base(36, 2, &res, "z", "1"); /* 'z' = 35, +1 = 36 = "10" */
        if (st == OK) { printf("sum base 36 = %s\n", res); free(res); }
        else fprintf(stderr, "Error %d\n", st);
    }

    /* Демонстрация 4: ошибка — цифра не из базы */
    {
        char *res = NULL;
        Status st = sum_in_base(10, 2, &res, "12", "3");
        printf("invalid digit test: st = %d (ERR_DIGIT = %d)\n", st, ERR_DIGIT);
        free(res);
    }
    return 0;
}