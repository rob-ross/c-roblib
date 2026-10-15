//
//  dev_utils.h
//
// Created by Rob Ross on 2/20/26.
//

#pragma once
#ifndef C_ROBLIB_DEV_UTILS_H
#define C_ROBLIB_DEV_UTILS_H

#include <stddef.h>
#include <time.h>


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
void repr_array_int(size_t n, const int array[], char const *prefix);
void du_repr_array_int_2D(size_t rows, size_t cols, int const array[][cols]);
#else
void repr_array_int(size_t n, const int array[static n], char const *prefix);
void du_repr_array_int_2D(size_t rows, size_t cols, int const array[static rows][cols]);
#endif

double du_elapsed_seconds(struct timespec start, struct timespec end);


#if !defined(print)
// expands to printf( fmt, ...)  but adds a newline to the end of fmt.
#define print(fmt, ...)                 \
    do {                                \
        printf((fmt), ##__VA_ARGS__);   \
        putchar('\n');                  \
    } while (0)
#endif

// todo (Rob 10-14-26) TIMEIT_START and TIMEIT_END don't appear to be used anywhere, and they won't work as is
// I suspect this was my early attempts to create a simple timing macro. 'start' and 'end' need to be visible in
// both blocks, but they are not visible from TIMEIT_END.
// `TIMEIT` appears to be the actual usable macro.
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

// Statement form, works in all compilers
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

// This form can be used as an expression. It will evaluate to
// double elapsed_time.
// Not all compilers support this form. It "returns" its result via the value of the last
// statement in the macro, `elapsed_time`.
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

#ifdef __cplusplus
}
#endif

#endif //C_ROBLIB_DEV_UTILS_H
