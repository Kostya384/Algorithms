/*
  Краткие требования:
  - C99, без глобальных переменных, без exit() вне main.
  - Валидация аргументов и файлов; корректная работа с памятью и обработка ошибок.
  - Разделение логики обработки данных и ввода/вывода.
  - Флаги: -d, -i, -s, -a, а также -nd, -ni, -ns, -na.
  - Если флаг не содержит n, выходной файл создаётся как out_<input_name>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum Status { OK = 0, ARG_ERR = 1, FLAG_ERR = 2, INPUT_ERR = 3, OUTPUT_ERR = 4, MEM_ERR = 5, IO_ERR = 6 };

static int is_letter(unsigned char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

static int is_digit(unsigned char ch) {
    return ch >= '0' && ch <= '9';
}

static int parse_flag(const char *flag, char *action, int *explicit_out) {
    size_t len;

    if (!flag || !action || !explicit_out) return ARG_ERR;
    if (flag[0] != '-' && flag[0] != '/') return FLAG_ERR;

    len = strlen(flag);
    if (len == 2 && (flag[1] == 'd' || flag[1] == 'i' || flag[1] == 's' || flag[1] == 'a')) {
        *action = flag[1];
        *explicit_out = 0;
        return OK;
    }

    if (len == 3 && flag[1] == 'n' &&
        (flag[2] == 'd' || flag[2] == 'i' || flag[2] == 's' || flag[2] == 'a')) {
        *action = flag[2];
        *explicit_out = 1;
        return OK;
    }

    return FLAG_ERR;
}

static char *make_out_path(const char *input_path) {
    const char *base = input_path;
    const char *sep = strrchr(input_path, '/');
    const char *bsep = strrchr(input_path, '\\');
    size_t dir_len = 0;
    size_t name_len;
    char *result;

    if (bsep && (!sep || bsep > sep)) sep = bsep;
    if (sep) {
        base = sep + 1;
        dir_len = (size_t)(sep - input_path + 1);
    }

    name_len = strlen(base);
    result = (char *)malloc(dir_len + 4 + name_len + 1);
    if (!result) return NULL;

    if (dir_len > 0) memcpy(result, input_path, dir_len);
    memcpy(result + dir_len, "out_", 4);
    memcpy(result + dir_len + 4, base, name_len + 1);
    return result;
}

static int remove_digits(const char *in, const char *out) {
    FILE *fin = fopen(in, "rb");
    FILE *fout = fopen(out, "wb");
    int ch;

    if (!fin) return INPUT_ERR;
    if (!fout) { fclose(fin); return OUTPUT_ERR; }

    while ((ch = fgetc(fin)) != EOF) {
        if (!is_digit((unsigned char)ch) && fputc(ch, fout) == EOF) {
            fclose(fin); fclose(fout); return IO_ERR;
        }
    }

    if (ferror(fin) || ferror(fout)) { fclose(fin); fclose(fout); return IO_ERR; }
    fclose(fin); fclose(fout);
    return OK;
}

static int count_line_op(const char *in, const char *out, char op) {
    FILE *fin = fopen(in, "r");
    FILE *fout = fopen(out, "w");
    char buf[4096];

    if (!fin) return INPUT_ERR;
    if (!fout) { fclose(fin); return OUTPUT_ERR; }

    while (fgets(buf, sizeof(buf), fin)) {
        size_t i, len = strlen(buf), count = 0;

        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) buf[--len] = '\0';

        for (i = 0; i < len; ++i) {
            unsigned char ch = (unsigned char)buf[i];
            if (op == 'i') {
                if (is_letter(ch)) ++count;
            } else if (op == 's') {
                if (ch != ' ' && !is_letter(ch) && !is_digit(ch)) ++count;
            }
        }

        if (fprintf(fout, "%zu\n", count) < 0) { fclose(fin); fclose(fout); return IO_ERR; }
    }

    if (ferror(fin) || ferror(fout)) { fclose(fin); fclose(fout); return IO_ERR; }
    fclose(fin); fclose(fout);
    return OK;
}

static int hex_encode(const char *in, const char *out) {
    FILE *fin = fopen(in, "rb");
    FILE *fout = fopen(out, "wb");
    int ch;

    if (!fin) return INPUT_ERR;
    if (!fout) { fclose(fin); return OUTPUT_ERR; }

    while ((ch = fgetc(fin)) != EOF) {
        if (is_digit((unsigned char)ch)) {
            if (fputc(ch, fout) == EOF) { fclose(fin); fclose(fout); return IO_ERR; }
        } else if (fprintf(fout, "%02X", (unsigned char)ch) < 0) {
            fclose(fin); fclose(fout); return IO_ERR;
        }
    }

    if (ferror(fin) || ferror(fout)) { fclose(fin); fclose(fout); return IO_ERR; }
    fclose(fin); fclose(fout);
    return OK;
}

static void usage(const char *prog) {
    if (!prog) return;
    fprintf(stderr,
            "Usage:\n"
            "  %s -d <input> [output]\n"
            "  %s -i <input> [output]\n"
            "  %s -s <input> [output]\n"
            "  %s -a <input> [output]\n"
            "  %s -nd <input> <output>\n"
            "  %s -ni <input> <output>\n"
            "  %s -ns <input> <output>\n"
            "  %s -na <input> <output>\n",
            prog, prog, prog, prog, prog, prog, prog, prog);
}

int main(int argc, char *argv[]) {
    char action = 0;
    int explicit_out = 0;
    const char *in = NULL;
    char *out = NULL;
    int status;

    if (argc < 3) { usage(argv[0]); return ARG_ERR; }

    status = parse_flag(argv[1], &action, &explicit_out);
    if (status != OK) { usage(argv[0]); return status; }

    if (explicit_out) {
        if (argc != 4) { usage(argv[0]); return ARG_ERR; }
        in = argv[2];
        out = argv[3];
    } else {
        if (argc != 3) { usage(argv[0]); return ARG_ERR; }
        in = argv[2];
        out = make_out_path(in);
        if (!out) return MEM_ERR;
    }

    if (action == 'd') status = remove_digits(in, out);
    else if (action == 'i' || action == 's') status = count_line_op(in, out, action);
    else if (action == 'a') status = hex_encode(in, out);
    else status = FLAG_ERR;

    if (!explicit_out) free(out);
    if (status != OK) fprintf(stderr, "Error code: %d\n", status);
    return status;
}
