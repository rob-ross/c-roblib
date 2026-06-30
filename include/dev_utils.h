//
//  dev_utils.h
//
// Created by Rob Ross on 2/20/26.
//

#pragma once

#include <stddef.h>
#include <time.h>


void repr_array_int(size_t n, const int array[static n], char const *prefix);
void du_repr_array_int_2D(size_t rows, size_t cols, int const array[static rows][ cols]);
double du_elapsed_seconds(struct timespec start, struct timespec end);


#if !defined(print)
// expands to printf( fmt, ...)  but adds a newline to the end of fmt.
#define print(fmt, ...)                 \
    do {                                \
        printf((fmt), ##__VA_ARGS__);   \
        putchar('\n');                  \
    } while (0)
#endif

#define TIMEIT_START                                    \
    do {                                                \
        struct timespec start;                          \
        struct timespec end;                            \
        double elapsed_time;                            \
        clock_gettime(CLOCK_MONOTONIC, &start);         \
    } while(0)

#define TIMEIT_END                                      \
    do {                                                \
        clock_gettime(CLOCK_MONOTONIC, &end);           \
        elapsed_time = du_elapsed_seconds(start, end);  \
        printf( #S "  time: %.6g s, ", elapsed_time);   \
    } while(0)

// #define TIMEIT( S )                                     \
//     do {                                                \
//         struct timespec start;                          \
//         struct timespec end;                            \
//         double elapsed_time = 0;                        \
//         clock_gettime(CLOCK_MONOTONIC, &start);         \
//         ( S ) ;                                         \
//         clock_gettime(CLOCK_MONOTONIC, &end);           \
//         elapsed_time = du_elapsed_seconds(start, end);  \
//         printf( #S "  time: %.6f s, ", elapsed_time);   \
//         elapsed_time;                                   \
//     } while (0)

#define TIMEIT( S )                                     \
    __extension__ ({                                    \
        struct timespec start;                          \
        struct timespec end;                            \
        double elapsed_time = 0;                        \
        clock_gettime(CLOCK_MONOTONIC, &start);         \
        ( S ) ;                                         \
        clock_gettime(CLOCK_MONOTONIC, &end);           \
        elapsed_time = du_elapsed_seconds(start, end);  \
        printf( #S "  time: %.6f s, ", elapsed_time);   \
        elapsed_time;                                   \
    } )
