#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum Status {
    STATUS_OK = 0,
    STATUS_INVALID_ARGS = 1,
    STATUS_INVALID_INPUT = 2,
    STATUS_MEMORY_ERROR = 3,
    STATUS_IO_ERROR = 4,
    STATUS_BAD_BASE = 5
};

static int is_letter(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

static int is_digit_char(char ch) {
    return ch >= '0' && ch <= '9';
}

static int digit_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 10;
    return -1;
}

static char *reverse_string(char *s) {
    size_t len = strlen(s);
    size_t i;

    for (i = 0; i < len / 2; ++i) {
        char tmp = s[i];
        s[i] = s[len - 1 - i];
        s[len - 1 - i] = tmp;
    }
    return s;
}

static void ull_to_base(unsigned long long value, int base, char *buffer) {
    static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    size_t pos = 0;

    if (base < 2 || base > 36) {
        buffer[0] = '\0';
        return;
    }

    do {
        buffer[pos++] = digits[value % (unsigned long long)base];
        value /= (unsigned long long)base;
    } while (value > 0ULL);

    buffer[pos] = '\0';
    reverse_string(buffer);
}

static unsigned long long parse_u64_base(const char *s, int base) {
    unsigned long long value = 0ULL;
    size_t i;

    if (!s || base < 2 || base > 36) return 0ULL;

    for (i = 0; s[i] != '\0'; ++i) {
        int d = digit_value(s[i]);
        if (d < 0 || d >= base) return 0ULL;
        value = value * (unsigned long long)base + (unsigned long long)d;
    }

    return value;
}

static int is_kaprekar_value(unsigned long long value, int base) {
    char square_buf[256];
    unsigned long long square = value * value;
    size_t len;
    size_t split;

    ull_to_base(square, base, square_buf);
    len = strlen(square_buf);

    if (len == 0) return 0;

    for (split = 0; split < len; ++split) {
        unsigned long long left = 0ULL;
        unsigned long long right = 0ULL;
        size_t i;

        for (i = 0; i < split; ++i) {
            left = left * (unsigned long long)base + (unsigned long long)digit_value(square_buf[i]);
        }

        for (i = split; i < len; ++i) {
            right = right * (unsigned long long)base + (unsigned long long)digit_value(square_buf[i]);
        }

        if (left + right == value) return 1;
    }

    return 0;
}

static int is_kaprekar_string(const char *s, int base) {
    unsigned long long value;
    size_t i;

    if (!s || s[0] == '\0') return 0;

    for (i = 0; s[i] != '\0'; ++i) {
        int d = digit_value(s[i]);
        if (d < 0 || d >= base) return 0;
    }

    value = parse_u64_base(s, base);
    return is_kaprekar_value(value, base);
}

static int is_convex_polygon(int count, double eps, ...) {
    va_list ap;
    double x[64];
    double y[64];
    int i;
    int sign = 0;

    if (count < 3 || count > 64) return 0;

    va_start(ap, eps);
    for (i = 0; i < count; ++i) {
        x[i] = va_arg(ap, double);
        y[i] = va_arg(ap, double);
    }
    va_end(ap);

    for (i = 0; i < count; ++i) {
        int j = (i + 1) % count;
        int k = (i + 2) % count;
        double dx1 = x[j] - x[i];
        double dy1 = y[j] - y[i];
        double dx2 = x[k] - x[j];
        double dy2 = y[k] - y[j];
        double cross = dx1 * dy2 - dy1 * dx2;

        if (cross > eps) {
            if (sign == -1) return 0;
            sign = 1;
        } else if (cross < -eps) {
            if (sign == 1) return 0;
            sign = -1;
        }
    }

    return sign != 0;
}

static double polynomial_value(double x, int degree, ...) {
    va_list ap;
    double result = 0.0;
    int i;

    if (degree < 0) return 0.0;

    va_start(ap, degree);
    for (i = 0; i <= degree; ++i) {
        result = result * x + va_arg(ap, double);
    }
    va_end(ap);

    return result;
}

static int count_kaprekar_in_base(int base, int count, ...) {
    va_list ap;
    int i;
    int total = 0;

    if (base < 2 || base > 36 || count < 0) return STATUS_INVALID_INPUT;

    va_start(ap, count);
    for (i = 0; i < count; ++i) {
        const char *s = va_arg(ap, const char *);
        if (s && is_kaprekar_string(s, base)) ++total;
    }
    va_end(ap);

    return total;
}

static double geometric_mean(int count, ...) {
    va_list ap;
    double product = 1.0;
    int i;

    if (count <= 0) return 0.0;

    va_start(ap, count);
    for (i = 0; i < count; ++i) {
        double v = va_arg(ap, double);
        if (v <= 0.0) {
            va_end(ap);
            return 0.0;
        }
        product *= v;
    }
    va_end(ap);

    return pow(product, 1.0 / (double)count);
}

static double fast_pow(double x, int n) {
    if (n == 0) return 1.0;
    if (n < 0) return 1.0 / fast_pow(x, -n);
    if (n % 2 == 0) {
        double p = fast_pow(x, n / 2);
        return p * p;
    }
    return x * fast_pow(x, n - 1);
}

static double bisection(double left, double right, double eps, double (*f)(double)) {
    double a = left;
    double b = right;
    double mid;

    if (!f) return 0.0;
    if (f(a) == 0.0) return a;
    if (f(b) == 0.0) return b;
    if (f(a) * f(b) > 0.0) return NAN;

    while (fabs(b - a) > eps) {
        mid = (a + b) / 2.0;
        if (f(a) * f(mid) <= 0.0) {
            b = mid;
        } else {
            a = mid;
        }
    }

    return (a + b) / 2.0;
}

static double eq_x2_minus_2(double x) {
    return x * x - 2.0;
}

static double eq_sin(double x) {
    return sin(x);
}

static double eq_cube(double x) {
    return x * x * x - 3.0 * x + 1.0;
}

int main(void) {
    printf("1) Convex polygon check:\n");
    printf("   convex: %s\n", is_convex_polygon(4, 1e-9,
        0.0, 0.0,
        2.0, 0.0,
        2.0, 2.0,
        0.0, 2.0) ? "YES" : "NO");
    printf("   concave: %s\n", is_convex_polygon(4, 1e-9,
        0.0, 0.0,
        2.0, 0.0,
        1.0, 0.5,
        0.0, 2.0) ? "YES" : "NO");

    printf("\n2) Polynomial value:\n");
    printf("   P(2) for 1*x^3 - 5*x^2 + 4*x + 2 = %.4f\n",
           polynomial_value(2.0, 3, 1.0, -5.0, 4.0, 2.0));

    printf("\n3) Kaprekar numbers in base 10:\n");
    printf("   count = %d\n",
           count_kaprekar_in_base(10, 6, "1", "9", "45", "99", "297", "123"));

    printf("\n4) Geometric mean:\n");
    printf("   GM = %.4f\n", geometric_mean(4, 1.0, 4.0, 16.0, 64.0));

    printf("\n5) Fast exponentiation:\n");
    printf("   2^10 = %.0f\n", fast_pow(2.0, 10));
    printf("   3^5 = %.0f\n", fast_pow(3.0, 5));

    printf("\n6) Bisection method:\n");
    printf("   x^2 - 2 = 0 on [1,2] => %.10f\n", bisection(1.0, 2.0, 1e-9, eq_x2_minus_2));
    printf("   sin(x) = 0 on [3,4] => %.10f\n", bisection(3.0, 4.0, 1e-9, eq_sin));
    printf("   x^3 - 3x + 1 = 0 on [0,1] => %.10f\n", bisection(0.0, 1.0, 1e-9, eq_cube));

    return STATUS_OK;
}
