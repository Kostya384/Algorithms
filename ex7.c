#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum Status {
    STATUS_OK = 0,
    STATUS_INVALID_ARGS = 1,
    STATUS_INVALID_FLAG = 2,
    STATUS_OPEN_INPUT_ERROR = 3,
    STATUS_OPEN_OUTPUT_ERROR = 4,
    STATUS_MEMORY_ERROR = 5,
    STATUS_IO_ERROR = 6
};

static int is_space_char(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

static void to_lower_ascii(char *s) {
    size_t i;
    for (i = 0; s[i] != '\0'; ++i) {
        if (s[i] >= 'A' && s[i] <= 'Z') {
            s[i] = (char)(s[i] + ('a' - 'A'));
        }
    }
}

static void to_base_n_string(char *s, int base) {
    unsigned char bytes[512];
    size_t len = strlen(s);
    size_t i, pos = 0;
    char buffer[512];
    size_t out = 0;

    if (base < 2 || base > 36 || len == 0) {
        s[0] = '\0';
        return;
    }

    for (i = 0; i < len; ++i) {
        bytes[i] = (unsigned char)s[i];
    }

    for (i = 0; i < len; ++i) {
        unsigned int code = bytes[i];
        char digits[64];
        size_t p = 0;

        if (code == 0) {
            digits[p++] = '0';
        } else {
            while (code > 0) {
                unsigned int rem = code % (unsigned int)base;
                digits[p++] = (char)(rem < 10 ? ('0' + rem) : ('A' + rem - 10));
                code /= (unsigned int)base;
            }
        }

        while (p > 0) {
            buffer[out++] = digits[--p];
        }
        if (i + 1 < len) {
            buffer[out++] = ' ';
        }
    }

    buffer[out] = '\0';
    strcpy(s, buffer);
}

static int parse_flag(const char *flag) {
    if (!flag) return STATUS_INVALID_FLAG;
    if (flag[0] != '-' && flag[0] != '/') return STATUS_INVALID_FLAG;
    if (strcmp(flag, "-r") == 0 || strcmp(flag, "/r") == 0) return STATUS_OK;
    if (strcmp(flag, "-a") == 0 || strcmp(flag, "/a") == 0) return STATUS_OK;
    return STATUS_INVALID_FLAG;
}

static int read_lexemes_from_file(const char *path, char ***items, int *count) {
    FILE *f = fopen(path, "r");
    char temp[4096];
    char *token;
    char *buffer = NULL;
    size_t total_len = 0;
    int item_count = 0;
    char **arr = NULL;

    if (!f) return STATUS_OPEN_INPUT_ERROR;

    while (fscanf(f, "%4095s", temp) == 1) {
        size_t len = strlen(temp);
        char *copy = (char *)malloc(len + 1);
        if (!copy) {
            for (int i = 0; i < item_count; ++i) free(arr[i]);
            free(arr);
            free(buffer);
            fclose(f);
            return STATUS_MEMORY_ERROR;
        }
        strcpy(copy, temp);

        if (item_count == 0) {
            arr = (char **)malloc(sizeof(char *));
        } else {
            char **tmp = (char **)realloc(arr, (size_t)(item_count + 1) * sizeof(char *));
            if (!tmp) {
                free(copy);
                for (int i = 0; i < item_count; ++i) free(arr[i]);
                free(arr);
                fclose(f);
                return STATUS_MEMORY_ERROR;
            }
            arr = tmp;
        }

        arr[item_count++] = copy;
        total_len += len + 1;
    }

    fclose(f);
    *items = arr;
    *count = item_count;
    return STATUS_OK;
}

static void free_items(char **items, int count) {
    int i;
    if (!items) return;
    for (i = 0; i < count; ++i) {
        free(items[i]);
    }
    free(items);
}

static int write_result_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return STATUS_OPEN_OUTPUT_ERROR;
    if (fprintf(f, "%s", content) < 0) {
        fclose(f);
        return STATUS_IO_ERROR;
    }
    fclose(f);
    return STATUS_OK;
}

static int task_r(const char *file1, const char *file2, const char *out) {
    char **arr1 = NULL, **arr2 = NULL;
    int count1 = 0, count2 = 0;
    int i, j, max;
    size_t total = 0;
    char *result = NULL;
    int status;

    status = read_lexemes_from_file(file1, &arr1, &count1);
    if (status != STATUS_OK) return status;

    status = read_lexemes_from_file(file2, &arr2, &count2);
    if (status != STATUS_OK) {
        free_items(arr1, count1);
        return status;
    }

    max = count1 > count2 ? count1 : count2;
    for (i = 0; i < max; ++i) {
        if (i < count1) total += strlen(arr1[i]) + 1;
        if (i < count2) total += strlen(arr2[i]) + 1;
    }

    result = (char *)malloc(total + 1);
    if (!result) {
        free_items(arr1, count1);
        free_items(arr2, count2);
        return STATUS_MEMORY_ERROR;
    }
    result[0] = '\0';

    for (i = 0; i < max; ++i) {
        if (i < count1) {
            char *tmp = result + strlen(result);
            sprintf(tmp, "%s%s", (result[0] == '\0') ? "" : " ", arr1[i]);
        }
        if (i < count2) {
            char *tmp = result + strlen(result);
            sprintf(tmp, "%s%s", (result[0] == '\0') ? "" : " ", arr2[i]);
        }
    }

    status = write_result_file(out, result);
    free(result);
    free_items(arr1, count1);
    free_items(arr2, count2);
    return status;
}

static void transform_word_ascii_base4(char *token) {
    size_t i;
    char out[4096];
    size_t pos = 0;

    to_lower_ascii(token);

    for (i = 0; token[i] != '\0'; ++i) {
        unsigned int code = (unsigned char)token[i];
        char digits[64];
        size_t p = 0;

        if (code == 0) {
            digits[p++] = '0';
        } else {
            while (code > 0) {
                unsigned int rem = code % 4U;
                digits[p++] = (char)(rem < 10 ? ('0' + rem) : ('A' + rem - 10));
                code /= 4U;
            }
        }

        while (p > 0) {
            out[pos++] = digits[--p];
        }
    }
    out[pos] = '\0';
    strcpy(token, out);
}

static void transform_word_lower(char *token) {
    to_lower_ascii(token);
}

static void transform_word_octal_ascii(char *token) {
    size_t i;
    char out[4096];
    size_t pos = 0;

    for (i = 0; token[i] != '\0'; ++i) {
        unsigned int code = (unsigned char)token[i];
        char digits[64];
        size_t p = 0;

        if (code == 0) {
            digits[p++] = '0';
        } else {
            while (code > 0) {
                digits[p++] = (char)('0' + (code % 8U));
                code /= 8U;
            }
        }

        while (p > 0) {
            out[pos++] = digits[--p];
        }
    }
    out[pos] = '\0';
    strcpy(token, out);
}

static int task_a(const char *in, const char *out) {
    FILE *fin = fopen(in, "r");
    FILE *fout = fopen(out, "w");
    char token[4096];
    int index = 0;
    int status = STATUS_OK;

    if (!fin) return STATUS_OPEN_INPUT_ERROR;
    if (!fout) {
        fclose(fin);
        return STATUS_OPEN_OUTPUT_ERROR;
    }

    while (fscanf(fin, "%4095s", token) == 1) {
        ++index;

        if (index % 10 == 0) {
            transform_word_ascii_base4(token);
        } else if (index % 2 == 0) {
            transform_word_lower(token);
        } else if (index % 5 == 0) {
            transform_word_octal_ascii(token);
        }

        if (fprintf(fout, "%s%s", (index == 1) ? "" : " ", token) < 0) {
            status = STATUS_IO_ERROR;
            break;
        }
    }

    if (ferror(fin) || ferror(fout)) status = STATUS_IO_ERROR;
    fclose(fin);
    fclose(fout);
    return status;
}

static void usage(const char *prog) {
    if (!prog) return;
    fprintf(stderr,
            "Usage:\n"
            "  %s -r <file1> <file2> <output>\n"
            "  %s -a <input> <output>\n",
            prog, prog);
}

int main(int argc, char *argv[]) {
    int status;
    const char *flag;

    if (argc < 2) {
        usage(argv[0]);
        return STATUS_INVALID_ARGS;
    }

    flag = argv[1];
    status = parse_flag(flag);
    if (status != STATUS_OK) {
        usage(argv[0]);
        return status;
    }

    if (strcmp(flag, "-r") == 0 || strcmp(flag, "/r") == 0) {
        if (argc != 5) {
            usage(argv[0]);
            return STATUS_INVALID_ARGS;
        }
        return task_r(argv[2], argv[3], argv[4]);
    }

    if (strcmp(flag, "-a") == 0 || strcmp(flag, "/a") == 0) {
        if (argc != 4) {
            usage(argv[0]);
            return STATUS_INVALID_ARGS;
        }
        return task_a(argv[2], argv[3]);
    }

    usage(argv[0]);
    return STATUS_INVALID_FLAG;
}
