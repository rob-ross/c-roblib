//
// Created by Rob Ross on 2/20/26.
//
// utility methods useful during development


#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "dev_utils.h"

#include <time.h>


char const *create_string(char const *fmt, ...) {

    va_list args;
    va_start(args, fmt);
    const int sz = vsnprintf(nullptr, 0, fmt, args);
    va_end(args);

    if (sz <= 0) {
        return strdup("");
    }
    const size_t buff_len =  sizeof(char) * (size_t)sz + 1;
    char *new_string = malloc(buff_len);
    if (!new_string) {
        return strdup("");
    }

    va_start(args, fmt);
    int n = vsnprintf(new_string, buff_len, fmt, args);
    va_end(args);
    if (n < sz) {
        printf("n < sz in create_string.\n");
    }

    return new_string;
}

void repr_array_int(size_t const n, const int array[static n], char const *prefix) {
    const char * new_string = nullptr;
    char const * prefix_ = nullptr;
    if (prefix) prefix_ = prefix;
    else {
        new_string = create_string("int[%zu]", n);
        prefix_ = new_string;
    }
    printf("%s",prefix_);
    printf("{");
    if (n > 0) {
        printf(" %d", array[0]);
    }
    for (size_t i = 1; i < n; ++i) {
        printf(", %d", array[i]);
    }

    printf(" }");

    if (new_string) {
        free((void *)new_string);
    }

}

// 4 blanks per tab
#define DU_TAB "    "
void du_repr_array_int_2D(size_t const rows, size_t const cols, int const array[static rows][ cols]) {
    printf("int[%zu][%zu]{\n", rows, cols);
    for (size_t r = 0; r < rows; ++r ) {
        char const *new_string = create_string("int[%zu]",r);
        printf(DU_TAB);
        repr_array_int(cols, array[r], new_string);
        putchar('\n');
        if (new_string) {
            free((void*)new_string);
        }
    }

    printf("\n}");
}

double du_elapsed_seconds(struct timespec start, struct timespec end) {
    return (double)(end.tv_sec - start.tv_sec)
         + (double)(end.tv_nsec - start.tv_nsec) / 1e9;
}
