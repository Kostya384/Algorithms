#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum Status {
    STATUS_OK = 0,
    STATUS_INVALID_BASE = 1,
    STATUS_INVALID_INPUT = 2,
    STATUS_MEMORY_ERROR = 3
};

static int digit_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 10;
    return -1;
}

static int is_valid_number_in_base(const char *s, int base) {
    size_t i;
    if (!s || s[0] == '\0') return 0;

    for (i = 0; s[i] != '\0'; ++i) {
        int v = digit_value(s[i]);
        if (v < 0 || v >= base) return 0;
    }

    return 1;
}

static void trim_leading_zeros(char *s) {
    size_t i, start = 0, len;
    char *out;

    if (!s || s[0] == '\0') return;

    while (s[start] == '0' && s[start + 1] != '\0') {
        ++start;
    }

    if (start == 0) return;

    len = strlen(s + start);
    out = (char *)malloc(len + 1);
    if (!out) return;

    for (i = 0; i < len; ++i) {
        out[i] = s[start + i];
    }
    out[len] = '\0';
    strcpy(s, out);
    free(out);
}

static long long parse_signed_number(const char *s, int base) {
    long long value = 0;
    size_t i = 0;
    int sign = 1;

    if (!s || s[0] == '\0') return 0;

    if (s[0] == '-') {
        sign = -1;
        ++i;
    }

    for (; s[i] != '\0'; ++i) {
        int d = digit_value(s[i]);
        if (d < 0 || d >= base) return 0;
        value = value * base + d;
    }

    return sign * value;
}

static void reverse_string(char *s) {
    size_t i, len = strlen(s);
    for (i = 0; i < len / 2; ++i) {
        char tmp = s[i];
        s[i] = s[len - 1 - i];
        s[len - 1 - i] = tmp;
    }
}

static void to_base_string(long long value, int base, char *out) {
    static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    unsigned long long u;
    size_t pos = 0;
    int negative = 0;

    if (base < 2 || base > 36) {
        out[0] = '\0';
        return;
    }

    if (value < 0) {
        negative = 1;
        u = (unsigned long long)(-(value + 1)) + 1ULL;
    } else {
        u = (unsigned long long)value;
    }

    if (u == 0ULL) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (u > 0ULL) {
        unsigned long long rem = u % (unsigned long long)base;
        out[pos++] = digits[(size_t)rem];
        u /= (unsigned long long)base;
    }

    if (negative) {
        out[pos++] = '-';
    }

    out[pos] = '\0';
    reverse_string(out);
}

static unsigned long long abs_u64(long long x) {
    return x < 0 ? (unsigned long long)(-(x + 1)) + 1ULL : (unsigned long long)x;
}

static int compare_abs(long long a, long long b) {
    unsigned long long ua = abs_u64(a);
    unsigned long long ub = abs_u64(b);
    return ua > ub ? 1 : (ua < ub ? -1 : 0);
}

int main(void) {
    int base;
    char input[256];
    long long sum = 0;
    long long max_abs_value = 0;
    char max_abs_str[256] = "0";
    char sum_str[256] = "0";
    int count = 0;

    printf("Enter base [2..36]: ");
    if (scanf("%d", &base) != 1 || base < 2 || base > 36) {
        fprintf(stderr, "Invalid base. Must be in [2..36].\n");
        return STATUS_INVALID_BASE;
    }

    while (1) {
        char line[256];
        long long current;

        printf("Enter number or Stop: ");
        if (scanf("%255s", line) != 1) {
            fprintf(stderr, "Input error.\n");
            return STATUS_INVALID_INPUT;
        }

        if (strcmp(line, "Stop") == 0 || strcmp(line, "stop") == 0) {
            break;
        }

        if (!is_valid_number_in_base(line, base)) {
            fprintf(stderr, "Invalid number for base %d: %s\n", base, line);
            continue;
        }

        current = parse_signed_number(line, base);
        sum += current;
        ++count;

        if (count == 1 || compare_abs(current, max_abs_value) > 0) {
            max_abs_value = current;
            strcpy(max_abs_str, line);
            trim_leading_zeros(max_abs_str);
        }
    }

    if (count == 0) {
        printf("No numbers entered.\n");
        return STATUS_OK;
    }

    to_base_string(sum, 9, sum_str);
    printf("Sum = %lld\n", sum);
    printf("Sum in base 9: %s\n", sum_str);

    printf("Maximum by absolute value = %lld\n", max_abs_value);
    printf("Max absolute value as original text: %s\n", max_abs_str);

    {
        char result[256];
        int bases[] = {9, 18, 27, 36};
        int i;

        for (i = 0; i < 4; ++i) {
            to_base_string(max_abs_value, bases[i], result);
            printf("Max abs in base %d: %s\n", bases[i], result);
        }

        printf("Sum representations:\n");
        for (i = 0; i < 4; ++i) {
            to_base_string(sum, bases[i], result);
            printf("  base %d: %s\n", bases[i], result);
        }
    }

    return STATUS_OK;
}
