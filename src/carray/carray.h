//
// Created by Rob Ross on 2/24/26.
//

#pragma once
#include <stddef.h>



typedef enum CArrayError {
    CARRAY_OK = 0,
    CARRAY_ERR_NULL_ARG,
    CARRAY_ERR_OUT_OF_MEM,
    CARRAY_ERR_EMPTY,
    CARRAY_ERR_INDEX_OUT_OF_RANGE
} CArrayError;

/**
 * @param err error code returned by Array functions
 * @return human-readable string for the error code
 */
const char *carray_err_str(CArrayError err);

/**
 * Returns the element of `array` at `index` in the `out` parameter. Returns CARRAY_OK on success, or an error value
 * in case of failure. If the method does not return CARRAY_OK, the `out` parameter is not changed.
 * @param n number of elements in `array`
 * @param array the array from which to get the element
 * @param index the index position of the array. If greater than n-1, returns CARRAY_ERR_INDEX_OUT_OF_RANGE
 * @param out either the value at array[index], or unchanged if error occurrs.
 * @return the value at element array[index] in the `out` parameter. This method returns CARRAY_OK on success
 * and an error code on failure.
 */
CArrayError carray_get_int(size_t n, const int array[static n], size_t index, int *out);

/**
 * Similar to carray_get_int, but if that function returns an error code, the fallback value is returned in `out`.
 * This method always returns CARRAY_OK.
 * @param n number of elements in `array`
 * @param array the array from which to get the element
 * @param index the index position of the array. If greater than n-1, returns `fallback` in `out`
 * @param out either the value at array[index], or `fallback` if error occurrs.
 * @param fallback the value returned in `out` if an error occurrs.
 * @return the value at element array[index] in the `out` parameter or the fallback value if an error occurred.
 */
CArrayError carray_get_or_int( size_t n, const int array[static n], size_t index, int *out, int fallback);

/**
 * Prints a representation of the array via printf, on the current line. Does not print a newline.
 * If `limit` is greater than 0, uses this to limit the number of displayed elements to MIN(n, limit).
 * If `message` is not null, uses this string as the output prefix instead of array element type.
 * @param n number of elements in the array
 * @param array the array to display
 * @param message optional, if null prefixes output with array element type, otherwise uses this as the prefix
 * @param limit if > 0, restricts output to the first MIN(n, limit) array elements. If 0, limits to DEFAULT_LIMIT items.
 */
void carray_repr_int(size_t n, const int array[static n], char const *message, size_t limit);

/**
 *  Sets the value of array[index] to the `value` argument.
 * @param n number of elements in the array
 * @param array the array for which to set the value
 * @param index the index position of the array. If greater than n-1, returns CARRAY_ERR_INDEX_OUT_OF_RANGE
 * @param value the value to set at array[index]
 * @return CARRAY_OK on success, or error code on failure.
 */
CArrayError carray_set_int(const size_t n, int array[static n], size_t index, int value);