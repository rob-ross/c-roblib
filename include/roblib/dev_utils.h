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


#if 0
// not worth the possible name collisions for the functionality it provides (rob)
// expands to printf( fmt, ...)  but adds a newline to the end of fmt.
#define print(fmt, ...)                 \
    do {                                \
        printf((fmt), ##__VA_ARGS__);   \
        putchar('\n');                  \
    } while (0)
#endif

// todo (rob) implement versions that allow formatting options for the time metric, (i.e., seconds, ms, micros, etc.
/**
 *  Calculates the elapsed time to execute the statement in argument `S` and prints the elapsed time
 *  to stdout via printf. The time is formatted in seconds.
 *  This form can only be used as a statement. I.e., it cannot be an rvalue.
 *  All compilers support this form.
 *  The argument must be a statement.
 *  Example:
 *   TIMEIT_STATEMENT( < statement > );
 */
#define TIMEIT_STATEMENT( S )                           \
    do {                                                \
        struct timespec start;                          \
        struct timespec end;                            \
        double elapsed_time = 0;                        \
        clock_gettime(CLOCK_MONOTONIC, &start);         \
        ( S ) ;                                         \
        clock_gettime(CLOCK_MONOTONIC, &end);           \
        elapsed_time = du_elapsed_seconds(start, end);  \
        printf( #S "  time: %.6f s, ", elapsed_time);   \
        elapsed_time;                                   \
    } while (0)

// todo (Rob) implement option to supress printing, or to format time display as seconds, ms, micro seconds, etc.
/**
 *  This form can be used as an expression. It will evaluate to
 *  (double) elapsed_time.
 *  Not all compilers support this form. It "returns" its result via the value of the last
 *  statement in the macro, which is `elapsed_time`
 *  The argument must be a statement.
 *  Example:
 *      double elapsed_time = TIMEIT_EXPR( < statement > );
 */
#define TIMEIT_EXPR( S )                                \
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
