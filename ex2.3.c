#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
//#include <string.h>

typedef enum {
    STATUS_OK = 0,
    STATUS_ERROR_INVALID_ARGC,
    STATUS_ERROR_INVALID_FILE_COUNT,
    STATUS_ERROR_EMPTY_SUBSTRING,
    STATUS_ERROR_FILE_NOT_FOUND,
    STATUS_ERROR_PARSE_INT,
    STATUS_ERROR_INVALID_PARAM,
    STATUS_ERROR_MEMORY_ALLOCATION,
    STATUS_ERROR_FILE_OPEN
} status_code_t;

typedef struct {
    int line;
    int column;
} match_position_t;

size_t custom_strlen(const char *str) {
    if (str == NULL) {
        return 0;
    }
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

status_code_t validate_input(int argc, char *argv[], int *out_file_count) {
    // argv[0] - имя программы, argv[1] - подстрока, argv[2] - количество файлов, argv[3] - первый файл.
    if (argc < 4) {
        return STATUS_ERROR_INVALID_ARGC;
    }

    if (argv[1][0] == '\0') {
        return STATUS_ERROR_EMPTY_SUBSTRING;
    }

    char *endptr = NULL;
    long count = strtol(argv[2], &endptr, 10);
    
    if (*endptr != '\0' || endptr == argv[2]) {
        return STATUS_ERROR_PARSE_INT;
    }
    if (count <= 0) {
        return STATUS_ERROR_INVALID_FILE_COUNT;
    }

    // Проверяем, совпадает ли реальное количество аргументов с заявленным числом файлов
    if (argc != 3 + count) {
        return STATUS_ERROR_INVALID_ARGC;
    }

    // Валидация путей к файлам
    for (int i = 0; i < count; ++i) {
        char *file_path = argv[3 + i];
        FILE *file = fopen(file_path, "r");
        if (file == NULL) {
            return STATUS_ERROR_FILE_NOT_FOUND;
        }
        fclose(file);
    }

        // Деэкранирование управляющих символов (\n, \t) в подстроке
    char *src = argv[1];
    char *dst = argv[1];
    while (*src != '\0') {
        if (*src == '\\' && *(src + 1) != '\0') {
            if (*(src + 1) == 'n') {
                *dst = '\n';
                src += 2;
                dst++;
            } else if (*(src + 1) == 't') {
                *dst = '\t';
                src += 2;
                dst++;
            } else if (*(src + 1) == '\\') {
                *dst = '\\';
                src += 2;
                dst++;
            } else {
                // Если это какой-то другой экранированный символ, оставляем слэш
                *dst = *src;
                src++;
                dst++;
            }
        } else {
            *dst = *src;
            src++;
            dst++;
        }
    }
    *dst = '\0'; // Не забываем закрыть строку терминальным нулем

    *out_file_count = (int)count;
    return STATUS_OK;
}

status_code_t calculate_kmp_table(const char *substr, int *arr, size_t len) {
    if (substr == NULL || arr == NULL || len == 0) {
        return STATUS_ERROR_INVALID_PARAM;
    }

    arr[0] = 0;
    size_t j = 0;

    for (size_t i = 1; i < len; ++i) {
        while (j > 0 && substr[i] != substr[j]) {
            j = (size_t)arr[j - 1];
        }
        if (substr[i] == substr[j]) {
            j++;
        }
        arr[i] = (int)j;
    }
    return STATUS_OK;
}
// Функция посимвольного поиска подстроки в одном файле.
// Принимает: подстроку, имя файла, готовую таблицу КМП, длину подстроки.
// Возвращает: через указатели динамический массив найденных позиций и их количество.
status_code_t search_substr(const char *substr, const char *file_name, const int *tabl_kmp, 
                            size_t len_sub, match_position_t **out_matches, size_t *out_count) {
    if (substr == NULL || file_name == NULL || tabl_kmp == NULL || out_matches == NULL || out_count == NULL) {
        return STATUS_ERROR_INVALID_PARAM;
    }

    FILE *file = fopen(file_name, "r");
    if (file == NULL) {
        return STATUS_ERROR_FILE_OPEN;
    }

    *out_matches = NULL;
    *out_count = 0;
    size_t capacity = 0;

    int ch;
    size_t stat = 0;         
    int current_line = 1;    
    int current_column = 0;  

    // Выделяем кольцевой буфер координат
    match_position_t *coord_buffer = malloc(len_sub * sizeof(match_position_t));
    if (coord_buffer == NULL) {
        fclose(file);
        return STATUS_ERROR_MEMORY_ALLOCATION;
    }
    size_t buffer_index = 0;

    status_code_t result_status = STATUS_OK;

    while ((ch = fgetc(file)) != EOF) {
        if (ch != '\n' && (ch < 128 || ch > 191)) {
            current_column++;
        }
        
        int log_col = (ch == '\n') ? (current_column + 1) : current_column;
        coord_buffer[buffer_index] = (match_position_t){current_line, log_col};
        
        match_position_t current_pos = coord_buffer[buffer_index];

        if (ch == '\n') {
            current_line++;
            current_column = 0;
        }

        // Логика отката КМП
        while (stat > 0 && ch != (unsigned char)substr[stat]) {
            stat = (size_t)tabl_kmp[stat - 1]; 
        }

        if (ch == (unsigned char)substr[stat]) {
            stat++;
        }

        // Фиксация совпадения
        if (stat == len_sub) {
            match_position_t start_pos;
            
            if (len_sub == 1) {
                // Если ищем один символ - старт совпадает с текущим символом
                start_pos = current_pos;
            } else {
                // Если подстрока длинная - берем координату из кольцевого буфера
                size_t start_idx = (buffer_index + 1) % len_sub;
                start_pos = coord_buffer[start_idx];
            }

            // Расширяем динамический массив результатов
            if (*out_count >= capacity) {
                capacity = capacity == 0 ? 4 : capacity * 2;
                match_position_t *temp = realloc(*out_matches, capacity * sizeof(match_position_t));
                if (temp == NULL) {
                    result_status = STATUS_ERROR_MEMORY_ALLOCATION;
                    break; 
                }
                *out_matches = temp;
            }

            (*out_matches)[(*out_count)++] = start_pos;
            
            // Сдвиг состояния КМП
            stat = (size_t)tabl_kmp[stat - 1];
        }

        buffer_index = (buffer_index + 1) % len_sub;
    }

    free(coord_buffer);
    fclose(file);

    if (result_status != STATUS_OK && *out_matches != NULL) {
        free(*out_matches);
        *out_matches = NULL;
        *out_count = 0;
    }

    return result_status;
}


/*status_code_t search_in_files(char *substr, int n, ...) {
    if(substr == NULL) return STATUS_ERROR_INVALID_PARAM;
    va_list ap;
    va_start(ap,n);

    for(int i = 0; i < n; i++) {
        char *file_name = va_arg(ap, char* );
        if (file_name == NULL) {
            va_end(ap);
            return STATUS_ERROR_INVALID_PARAM;
        }
        status_code_t code = search_substr(substr,file_name);
        if(code != 0) {
            va_end(ap);
            return code;
        }
    }
    va_end(ap);
    return STATUS_OK;
}*/

status_code_t vsearch_in_files(const char *substr, int n, ...) {
    if (substr == NULL || n <= 0) {
        return STATUS_ERROR_INVALID_PARAM;
    }

    size_t len_sub = custom_strlen(substr);
    int *tabl_kmp = malloc(len_sub * sizeof(int));
    if (tabl_kmp == NULL) {
        return STATUS_ERROR_MEMORY_ALLOCATION;
    }

    status_code_t code = calculate_kmp_table(substr, tabl_kmp, len_sub);
    if (code != STATUS_OK) {
        free(tabl_kmp);
        return code;
    }

    va_list ap;
    va_start(ap, n);

    char **files_array = va_arg(ap, char **);
    va_end(ap); 

    if (files_array == NULL) {
        free(tabl_kmp);
        return STATUS_ERROR_INVALID_PARAM;
    }

    for (int i = 0; i < n; i++) {
        const char *file_name = files_array[3 + i]; 
        if (file_name == NULL) {
            free(tabl_kmp);
            return STATUS_ERROR_INVALID_PARAM;
        }

        match_position_t *matches = NULL;
        size_t match_count = 0;

        code = search_substr(substr, file_name, tabl_kmp, len_sub, &matches, &match_count);
        if (code != STATUS_OK) {
            free(tabl_kmp);
            return code;
        }

        printf("Файл: %s\n", file_name);
        if (match_count == 0) {
            printf("  Вхождений не найдено.\n");
        } else {
            for (size_t m = 0; m < match_count; m++) {
                printf("  Найдено вхождение -> Строка: %d, Символ: %d\n", matches[m].line, matches[m].column);
            }
        }
        if (matches != NULL) {
            free(matches);
        }
    }

    free(tabl_kmp);
    return STATUS_OK;
}


/*status_code_t search_in_files(const char *substr, int n, ...) {
    if (substr == NULL || n <= 0) {
        return STATUS_ERROR_INVALID_PARAM;
    }
    va_list ap;
    va_start(ap, n); 

    status_code_t code = vsearch_in_files(substr, n, ap);

    va_end(ap);
    return code;
}*/


int main(int argc, char *argv[]) {
    int file_count = 0;
    
    status_code_t status = validate_input(argc, argv, &file_count);
    
    if (status != STATUS_OK) {
        switch (status) {
            case STATUS_ERROR_INVALID_ARGC:
                fprintf(stderr, "[ERROR] Неверное количество аргументов командной строки.\n");
                break;
            case STATUS_ERROR_EMPTY_SUBSTRING:
                fprintf(stderr, "[ERROR] Искомая подстрока не должна быть пустой.\n");
                break;
            case STATUS_ERROR_PARSE_INT:
                fprintf(stderr, "[ERROR] Не удалось распознать количество файлов как целое число.\n");
                break;
            case STATUS_ERROR_INVALID_FILE_COUNT:
                fprintf(stderr, "[ERROR] Количество файлов должно быть строго больше нуля.\n");
                break;
            case STATUS_ERROR_FILE_NOT_FOUND:
                fprintf(stderr, "[ERROR] Один или несколько файлов не найдены или недоступны.\n");
                break;
            default:
                fprintf(stderr, "[ERROR] Неизвестная ошибка валидации.\n");
                break;
        }
        return 1;
    }

    const char *substring = argv[1];

    status_code_t search_status = vsearch_in_files(substring, file_count, argv);

    if (search_status != STATUS_OK) {
        switch (search_status) {
            case STATUS_ERROR_INVALID_PARAM:
                fprintf(stderr, "[ERROR] Внутренняя ошибка: функции переданы неверные параметры.\n");
                break;
            case STATUS_ERROR_MEMORY_ALLOCATION:
                fprintf(stderr, "[ERROR] Недостаточно оперативной памяти для выполнения операции.\n");
                break;
            case STATUS_ERROR_FILE_OPEN:
                fprintf(stderr, "[ERROR] Не удалось открыть один из файлов во время чтения.\n");
                break;
            default:
                fprintf(stderr, "[ERROR] Критическая ошибка времени выполнения: %d\n", search_status);
                break;
        }
        return 1;
    }

    return 0;
}

