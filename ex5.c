#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static double my_abs(double x) {
    return x < 0.0 ? -x : x;
}

static double factorial_value(int n) {
    double result = 1.0;
    int i;

    if (n < 0) {
        return NAN;
    }

    for (i = 2; i <= n; ++i) {
        result *= (double)i;
    }
    return result;
}

static double bisection(double left, double right, double eps, double (*f)(double)) {
    double mid;

    if (!f) {
        return NAN;
    }

    if (f(left) == 0.0) {
        return left;
    }
    if (f(right) == 0.0) {
        return right;
    }
    if (f(left) * f(right) > 0.0) {
        return NAN;
    }

    while (my_abs(right - left) > eps) {
        mid = (left + right) / 2.0;
        if (f(left) * f(mid) <= 0.0) {
            right = mid;
        } else {
            left = mid;
        }
    }

    return (left + right) / 2.0;
}

static double f_ln_x_minus_1(double x) {
    return log(x) - 1.0;
}

static double f_cos_plus_1(double x) {
    return cos(x) + 1.0;
}

static double f_exp_minus_2(double x) {
    return exp(x) - 2.0;
}

static double f_x2_minus_2(double x) {
    return x * x - 2.0;
}

static double e_series(double eps) {
    double sum = 1.0;
    double term = 1.0;
    int n = 1;

    while (1) {
        term /= (double)n;
        if (my_abs(term) < eps) {
            break;
        }
        sum += term;
        ++n;
    }
    return sum;
}

static double e_limit(double eps) {
    int n = 1;
    double prev = 1.0;
    double current;

    while (1) {
        current = pow(1.0 + 1.0 / (double)n, (double)n);
        if (my_abs(current - prev) < eps) {
            return current;
        }
        prev = current;
        ++n;
    }
}

static double e_equation(double eps) {
    return bisection(1.0, 3.0, eps, f_ln_x_minus_1);
}

static double pi_series(double eps) {
    double sum = 0.0;
    double term = 4.0;
    int n = 1;

    while (my_abs(term) > eps) {
        sum += term;
        term *= -((2.0 * (double)n - 1.0) / (2.0 * (double)n + 1.0));
        ++n;
    }

    return sum;
}

static double pi_product(double eps) {
    double product = 1.0;
    double prev = 0.0;
    int n = 1;

    while (my_abs(product - prev) > eps) {
        prev = product;
        product *= (4.0 * (double)n * (double)n) / ((2.0 * (double)n - 1.0) * (2.0 * (double)n + 1.0));
        ++n;
    }

    return 2.0 * product;
}

static double pi_equation(double eps) {
    return bisection(2.0, 4.0, eps, f_cos_plus_1);
}

static double ln2_series(double eps) {
    double sum = 0.0;
    double term = 1.0;
    int n = 1;

    while (my_abs(term) > eps) {
        sum += term;
        term *= -((double)n / (double)(n + 1));
        ++n;
    }

    return sum;
}

static double ln2_limit(double eps) {
    int n = 1;
    double prev = 0.0;
    double current;

    while (1) {
        current = (double)n * (pow(2.0, 1.0 / (double)n) - 1.0);
        if (my_abs(current - prev) < eps) {
            return current;
        }
        prev = current;
        ++n;
    }
}

static double ln2_equation(double eps) {
    return bisection(0.0, 1.0, eps, f_exp_minus_2);
}

static double sqrt2_iteration(double eps) {
    double x = 1.0;
    double prev;

    do {
        prev = x;
        x = 0.5 * (x + 2.0 / x);
    } while (my_abs(x - prev) > eps);

    return x;
}

static double sqrt2_equation(double eps) {
    return bisection(1.0, 2.0, eps, f_x2_minus_2);
}

int main(int argc, char *argv[]) {
    int n;
    double eps;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <n> <epsilon>\n", argv[0]);
        return 1;
    }

    n = atoi(argv[1]);
    eps = atof(argv[2]);

    if (n < 0 || eps <= 0.0) {
        fprintf(stderr, "Invalid input: n >= 0 and epsilon > 0 required.\n");
        return 1;
    }

    printf("Factorial(%d) = %.0f\n\n", n, factorial_value(n));
    printf("Accuracy epsilon = %.10g\n\n", eps);

    printf("e (series)      = %.12f\n", e_series(eps));
    printf("e (limit)       = %.12f\n", e_limit(eps));
    printf("e (equation)    = %.12f\n\n", e_equation(eps));

    printf("pi (series)     = %.12f\n", pi_series(eps));
    printf("pi (product)    = %.12f\n", pi_product(eps));
    printf("pi (equation)   = %.12f\n\n", pi_equation(eps));

    printf("ln2 (series)    = %.12f\n", ln2_series(eps));
    printf("ln2 (limit)     = %.12f\n", ln2_limit(eps));
    printf("ln2 (equation)  = %.12f\n\n", ln2_equation(eps));

    printf("sqrt2 (iter)    = %.12f\n", sqrt2_iteration(eps));
    printf("sqrt2 (equation)= %.12f\n", sqrt2_equation(eps));

    return 0;
}
