#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum Status {
    STATUS_OK = 0,
    STATUS_INVALID_ARGS = 1,
    STATUS_MEMORY_ERROR = 2,
    STATUS_IO_ERROR = 3
};

static int rand_in_range(int a, int b) {
    int low = a < b ? a : b;
    int high = a < b ? b : a;
    return low + rand() % (high - low + 1);
}

static void fill_array(int *arr, size_t size, int a, int b) {
    size_t i;
    for (i = 0; i < size; ++i) {
        arr[i] = rand_in_range(a, b);
    }
}

static void swap_int(int *x, int *y) {
    int tmp = *x;
    *x = *y;
    *y = tmp;
}

static int minmax_swap_in_one_pass(int *arr, size_t size) {
    size_t i;
    int min_index = 0;
    int max_index = 0;

    if (!arr || size == 0) {
        return STATUS_INVALID_ARGS;
    }

    for (i = 1; i < size; ++i) {
        if (arr[i] < arr[min_index]) {
            min_index = (int)i;
        }
        if (arr[i] > arr[max_index]) {
            max_index = (int)i;
        }
    }

    if (min_index != max_index) {
        swap_int(&arr[min_index], &arr[max_index]);
    }

    return STATUS_OK;
}

static int *build_random_array(size_t *size_out) {
    size_t size;
    int *arr;

    if (!size_out) {
        return NULL;
    }

    size = (size_t)(rand_in_range(10, 10000));
    arr = (int *)malloc(size * sizeof(int));
    if (!arr) {
        return NULL;
    }

    for (size_t i = 0; i < size; ++i) {
        arr[i] = rand_in_range(-1000, 1000);
    }

    *size_out = size;
    return arr;
}

static int *build_array_c(const int *A, size_t sizeA, const int *B, size_t sizeB) {
    int *C;
    size_t i;

    if (!A || !B || sizeA == 0 || sizeB == 0) {
        return NULL;
    }

    C = (int *)malloc(sizeA * sizeof(int));
    if (!C) {
        return NULL;
    }

    for (i = 0; i < sizeA; ++i) {
        int best = B[0];
        int min_diff = abs(A[i] - B[0]);

        for (size_t j = 1; j < sizeB; ++j) {
            int diff = abs(A[i] - B[j]);
            if (diff < min_diff) {
                min_diff = diff;
                best = B[j];
            }
        }

        C[i] = A[i] + best;
    }

    return C;
}

static void print_array(const int *arr, size_t size) {
    size_t i;
    for (i = 0; i < size; ++i) {
        printf("%d%s", arr[i], (i + 1 < size) ? " " : "\n");
    }
}

int main(int argc, char *argv[]) {
    int a, b;
    size_t size = 10;
    int *arr = NULL;
    int *A = NULL, *B = NULL, *C = NULL;
    size_t sizeA, sizeB;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <a> <b>\n", argv[0]);
        return STATUS_INVALID_ARGS;
    }

    a = atoi(argv[1]);
    b = atoi(argv[2]);

    if (a > b) {
        int tmp = a;
        a = b;
        b = tmp;
    }

    srand((unsigned int)time(NULL));

    arr = (int *)malloc(size * sizeof(int));
    if (!arr) {
        fprintf(stderr, "Memory allocation failed\n");
        return STATUS_MEMORY_ERROR;
    }

    fill_array(arr, size, a, b);
    printf("Initial array: ");
    print_array(arr, size);

    if (minmax_swap_in_one_pass(arr, size) != STATUS_OK) {
        free(arr);
        return STATUS_INVALID_ARGS;
    }

    printf("After min/max swap: ");
    print_array(arr, size);

    A = build_random_array(&sizeA);
    B = build_random_array(&sizeB);
    if (!A || !B) {
        free(arr);
        free(A);
        free(B);
        fprintf(stderr, "Memory allocation failed\n");
        return STATUS_MEMORY_ERROR;
    }

    C = build_array_c(A, sizeA, B, sizeB);
    if (!C) {
        free(arr);
        free(A);
        free(B);
        fprintf(stderr, "Memory allocation failed\n");
        return STATUS_MEMORY_ERROR;
    }

    printf("A: ");
    print_array(A, sizeA);
    printf("B: ");
    print_array(B, sizeB);
    printf("C: ");
    print_array(C, sizeA);

    free(arr);
    free(A);
    free(B);
    free(C);
    return STATUS_OK;
}
