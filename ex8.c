#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum Status {
    STATUS_OK = 0,
    STATUS_INVALID_ARGS = 1,
    STATUS_OPEN_INPUT_ERROR = 2,
    STATUS_OPEN_OUTPUT_ERROR = 3,
    STATUS_MEMORY_ERROR = 4,
    STATUS_IO_ERROR = 5
};

static int is_separator(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

static int value_of_digit(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 10;
    return -1;
}

static int valid_in_base(const char *token, int base) {
    size_t i;
    if (!token || token[0] == '\0') return 0;

    for (i = 0; token[i] != '\0'; ++i) {
        int v = value_of_digit(token[i]);
        if (v < 0 || v >= base) return 0;
    }

    return 1;
}

static char *trim_leading_zeroes(const char *token) {
    size_t i = 0;
    size_t len;
    char *copy;
    size_t start = 0;

    if (!token || token[0] == '\0') {
        copy = (char *)malloc(1);
        if (!copy) return NULL;
        copy[0] = '\0';
        return copy;
    }

    while (token[start] == '0' && token[start + 1] != '\0') {
        ++start;
    }

    len = strlen(token + start);
    copy = (char *)malloc(len + 1);
    if (!copy) return NULL;

    for (i = 0; i < len; ++i) {
        copy[i] = token[start + i];
    }
    copy[len] = '\0';
    return copy;
}

static unsigned long long to_decimal(const char *token, int base) {
    unsigned long long value = 0ULL;
    size_t i;

    if (!token || !valid_in_base(token, base)) return 0ULL;

    for (i = 0; token[i] != '\0'; ++i) {
        value = value * (unsigned long long)base + (unsigned long long)value_of_digit(token[i]);
    }

    return value;
}

static int determine_min_base(const char *token) {
    int base;
    size_t i;
    char *trimmed;

    if (!token || token[0] == '\0') return 2;

    trimmed = trim_leading_zeroes(token);
    if (!trimmed) return 2;

    for (base = 2; base <= 36; ++base) {
        int valid = 1;
        for (i = 0; trimmed[i] != '\0'; ++i) {
            int v = value_of_digit(trimmed[i]);
            if (v < 0 || v >= base) {
                valid = 0;
                break;
            }
        }
        if (valid) {
            free(trimmed);
            return base;
        }
    }

    free(trimmed);
    return 36;
}

static int process_file(const char *input_path, const char *output_path) {
    FILE *in = fopen(input_path, "r");
    FILE *out = fopen(output_path, "w");
    char token[512];
    int status = STATUS_OK;

    if (!in) return STATUS_OPEN_INPUT_ERROR;
    if (!out) {
        fclose(in);
        return STATUS_OPEN_OUTPUT_ERROR;
    }

    while (fscanf(in, "%511s", token) == 1) {
        char *trimmed = trim_leading_zeroes(token);
        int base = determine_min_base(token);
        unsigned long long dec = to_decimal(trimmed ? trimmed : token, base);

        if (!trimmed) {
            fclose(in);
            fclose(out);
            return STATUS_MEMORY_ERROR;
        }

        if (fprintf(out, "%s %d %llu\n", trimmed, base, dec) < 0) {
            free(trimmed);
            status = STATUS_IO_ERROR;
            break;
        }

        free(trimmed);
    }

    if (ferror(in) || ferror(out)) {
        status = STATUS_IO_ERROR;
    }

    fclose(in);
    fclose(out);
    return status;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        return STATUS_INVALID_ARGS;
    }

    return process_file(argv[1], argv[2]);
}
