//
//  carray_template.inc
//
// Created by Rob Ross on 2/24/26.
//


// -----------------------------------
//
// DO NOT INCLUDE THIS FILE.
//
// this file is intended to ONLY be included by carray_types.h and carray_types.c.
// It generates implementations of an Array for
// various element types for use by generic programming, such as array_int, array_char, etc.
//
// -----------------------------------

// ReSharper disable once CppMissingIncludeGuard

#include <stdio.h>

#include "../base.h"
#include "carray_template.h"

// todo remove these ifndefs
// temp during development so IDE editor has a concrete type
#ifndef CARRAY_T
#   define CARRAY_T int
#endif
#ifndef CARRAY_NAME
#   define CARRAY_NAME _int
#endif


#if !defined(CARRAY_IMPLEMENTATION)
/**
 * Returns the element of `array` at `index` in the `out` parameter. Returns CARRAY_OK on success, or an error value
 * in case of failure. If the method does not return CARRAY_OK, the `out` parameter is not changed.
 * @param n size_t: number of elements in `array`
 * @param array the array from which to get the element
 * @param index size_t: the index position of the array. If greater than n-1, returns CARRAY_ERR_INDEX_OUT_OF_RANGE
 * @param out either the value at array[index], or unchanged if an error occurrs.
 * @return CArrayError: This method returns CARRAY_OK on success and copies array[index] to `out`.
 * An error code on failure. `out` is only modified on success
 */
CArrayError CARRAY_FN(_get)(size_t n, CARRAY_T const array[static n], size_t index, CARRAY_T out[static 1]) ;
#else
CArrayError CARRAY_FN(_get)(const size_t n, CARRAY_T const array[static n], const size_t index, CARRAY_T out[static 1]) {
    if (!array || !out) {
        return CARRAY_ERR_NULL_ARG;
    }
    if ( n == 0) {
        return CARRAY_ERR_EMPTY;
    }
    if (  index >= n ) {
        return CARRAY_ERR_INDEX_OUT_OF_RANGE;
    }
    CARRAY_T element = array[index];
    out[0] = element;
    return CARRAY_OK;
}
#endif


#if !defined(CARRAY_IMPLEMENTATION)
/**
 * Similar to get(), but if that function returns an error code, the fallback value is returned in `out`.
 * This method always returns CARRAY_OK.
 * @param n size_t: number of elements in `array`
 * @param array the array from which to get the element
 * @param index size_t: the index position of the array. If greater than n-1, returns `fallback` in `out`
 * @param out either the value at array[index], or `fallback` if error occurrs.
 * @param fallback the value returned in `out` if an error occurrs.
 * @return CArrayError: CARRAY_OK. Sets value of `out` to array[index] or uses the fallback value if an error occurred.
 */
CArrayError CARRAY_FN(_get_or)(size_t n, CARRAY_T const array[static n], size_t index, CARRAY_T out[static 1], CARRAY_T fallback) ;
#else
CArrayError CARRAY_FN(_get_or)(const size_t n, CARRAY_T const array[static n], const size_t index, CARRAY_T out[static 1], CARRAY_T  fallback ) {
    CArrayError error = CARRAY_FN(_get)(n, array, index, out);
    if (!error) {
        return CARRAY_OK;
    }
    *out = fallback;
    return CARRAY_OK;
}
#endif


#if !defined(CARRAY_IMPLEMENTATION)
/**
 * Prints a representation of the array via printf, on the current line. Does not print a newline.
 * If `limit` is greater than 0, uses this to limit the number of displayed elements to MIN(n, limit).
 * If `message` is not null, uses this string as the output prefix instead of array element type.
 * @param n size_t: number of elements in the array
 * @param array the array to display
 * @param message char*: optional, if null prefixes output with array element type, otherwise uses this as the prefix
 * @param limit size_t: if > 0, restricts output to the first MIN(n, limit) array elements.
 * If 0, limits to DEFAULT_LIMIT items.
 */
void CARRAY_FN(_repr)(size_t n, CARRAY_T const array[static n], char const *message, size_t limit) ;
#else
void CARRAY_FN(_repr)(const size_t n, CARRAY_T const array[static n], char const *message, const size_t limit) {
    const char *msg = message ? message : STRINGIFY(CARRAY_T);
    size_t num_to_display = n;
    if (limit == 0) {
        num_to_display = MIN(DEFAULT_LIMIT, n);
    } else {
        num_to_display = MIN(limit, n);
    }
    printf("(%s[%zu]){", msg, n);
    if (num_to_display > 0 ) {
        printf(" ");
        printf(CONVERSION_FMT(array[0]) , array[0] );
    }
    for (size_t index = 1; index < num_to_display; ++index) {
        printf(", ");
        printf(CONVERSION_FMT(array[0]) , array[index]);
    }
    if (num_to_display < n) {
        printf(", ...");
    }
    printf(" }");
}
#endif

#if !defined(CARRAY_IMPLEMENTATION)
/**
 * Sets the value of array[index] to the `value` argument.
 * @param n size_t: number of elements in the array
 * @param array the array for which to set the value
 * @param index size_t: the index position of the array. If greater than n-1, returns CARRAY_ERR_INDEX_OUT_OF_RANGE
 * @param value the value to set at array[index]
 * @return CArrayError: CARRAY_OK on success, or error code on failure.
 */
CArrayError CARRAY_FN(_set)( size_t n, CARRAY_T array[static n], size_t index, CARRAY_T value) ;
#else
CArrayError CARRAY_FN(_set)(const size_t n, CARRAY_T array[static n], const size_t index, CARRAY_T const value) {
    if (!array) {
        return CARRAY_ERR_NULL_ARG;
    }
    if ( n == 0) {
        return CARRAY_ERR_EMPTY;
    }
    if (  index >= n ) {
        return CARRAY_ERR_INDEX_OUT_OF_RANGE;
    }
    array[index] = value;
    return CARRAY_OK;
}
#endif



// todo remove after development.
#undef CARRAY_T
#undef CARRAY_NAME
