//
//  carray_types.h
//
// Created by Rob Ross on 2/24/26.
//

/**
*  This header defines macros get, set, repr.
*  If you need to avoid them, #undef after include
*/

#pragma once

#include "carray_template.h"

#define CARRAY_T bool
#define CARRAY_NAME _bool
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T bool*
#define CARRAY_NAME _bool_ptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T bool const*
#define CARRAY_NAME _bool_cptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T char
#define CARRAY_NAME _char
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T char*
#define CARRAY_NAME _char_ptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T char const*
#define CARRAY_NAME _char_cptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T unsigned char
#define CARRAY_NAME _uchar
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T unsigned char*
#define CARRAY_NAME _uchar_ptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T unsigned char const*
#define CARRAY_NAME _uchar_cptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T signed char
#define CARRAY_NAME _schar
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T signed char*
#define CARRAY_NAME _schar_ptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T signed char const*
#define CARRAY_NAME _schar_cptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T short
#define CARRAY_NAME _short
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T short*
#define CARRAY_NAME _short_ptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T short const*
#define CARRAY_NAME _short_cptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T unsigned short
#define CARRAY_NAME _ushort
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T unsigned short*
#define CARRAY_NAME _ushort_ptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T unsigned short const*
#define CARRAY_NAME _ushort_cptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T int
#define CARRAY_NAME _int
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T int*
#define CARRAY_NAME _int_ptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T int const*
#define CARRAY_NAME _int_cptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T unsigned int
#define CARRAY_NAME _uint
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T unsigned int*
#define CARRAY_NAME _uint_ptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T unsigned int const*
#define CARRAY_NAME _uint_cptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T long
#define CARRAY_NAME _long
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T long*
#define CARRAY_NAME _long_ptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T long const*
#define CARRAY_NAME _long_cptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T unsigned long
#define CARRAY_NAME _ulong
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T unsigned long*
#define CARRAY_NAME _ulong_ptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T unsigned long const*
#define CARRAY_NAME _ulong_cptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T long long
#define CARRAY_NAME _llong
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T long long*
#define CARRAY_NAME _llong_ptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T long long const*
#define CARRAY_NAME _llong_cptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T unsigned long long
#define CARRAY_NAME _ullong
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T unsigned long long*
#define CARRAY_NAME _ullong_ptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T unsigned long long const*
#define CARRAY_NAME _ullong_cptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T float
#define CARRAY_NAME _float
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T float*
#define CARRAY_NAME _float_ptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T float const*
#define CARRAY_NAME _float_cptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T double
#define CARRAY_NAME _double
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T double*
#define CARRAY_NAME _double_ptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T double const*
#define CARRAY_NAME _double_cptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T long double
#define CARRAY_NAME _ldouble
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T long double*
#define CARRAY_NAME _ldouble_ptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T long double const*
#define CARRAY_NAME _ldouble_cptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T void*
#define CARRAY_NAME _void_ptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME

#define CARRAY_T void const*
#define CARRAY_NAME _void_cptr
#include "carray_template.inc"
#undef CARRAY_T
#undef CARRAY_NAME



// ------------------------------------------------------------
// Type routing table (single source of truth for _Generic maps)
// ------------------------------------------------------------
#define CARRAY_ELEM_TYPES(X)    \
    X(bool,                       _bool )                      \
    X(bool*,                      _bool_ptr )                  \
    X(bool const*,                _bool_cptr )                 \
    X(char,                       _char )                      \
    X(char*,                      _char_ptr )                  \
    X(char const*,                _char_cptr )                 \
    X(unsigned char,              _uchar )                     \
    X(unsigned char*,             _uchar_ptr )                 \
    X(unsigned char const*,       _uchar_cptr )                \
    X(signed char,                _schar )                     \
    X(signed char*,               _schar_ptr )                 \
    X(signed char const*,         _schar_cptr )                \
    X(short,                      _short )                     \
    X(short*,                     _short_ptr )                 \
    X(short const*,               _short_cptr )                \
    X(unsigned short,             _ushort )                    \
    X(unsigned short*,            _ushort_ptr )                \
    X(unsigned short const*,      _ushort_cptr )               \
    X(int,                        _int )                       \
    X(int*,                       _int_ptr )                   \
    X(int const*,                 _int_cptr )                  \
    X(unsigned int,               _uint )                      \
    X(unsigned int*,              _uint_ptr )                  \
    X(unsigned int const*,        _uint_cptr )                 \
    X(long,                       _long )                      \
    X(long*,                      _long_ptr )                  \
    X(long const*,                _long_cptr )                 \
    X(unsigned long,              _ulong )                     \
    X(unsigned long*,             _ulong_ptr )                 \
    X(unsigned long const*,       _ulong_cptr )                \
    X(long long,                  _llong )                     \
    X(long long*,                 _llong_ptr )                 \
    X(long long const*,           _llong_cptr )                \
    X(unsigned long long,         _ullong )                    \
    X(unsigned long long*,        _ullong_ptr )                \
    X(unsigned long long const*,  _ullong_cptr )               \
    X(float,                      _float )                     \
    X(float*,                     _float_ptr )                 \
    X(float const*,               _float_cptr )                \
    X(double,                     _double )                    \
    X(double*,                    _double_ptr )                \
    X(double const*,              _double_cptr )               \
    X(long double,                _ldouble )                   \
    X(long double*,               _ldouble_ptr )               \
    X(long double const*,         _ldouble_cptr )              \
    X(void*,                      _void_ptr )                  \
    X(void const*,                _void_cptr )


// ---------------
// method dispatcher
// ---------------
#define CARRAY__GET_CASE(T, SUFFIX)     T: CAT(carray_get, SUFFIX),
#define CARRAY__GET_OR_CASE(T, SUFFIX)  T: CAT(carray_get_or, SUFFIX),
#define CARRAY__REPR_CASE(T, SUFFIX)    T: CAT(carray_repr, SUFFIX),
#define CARRAY__SET_CASE(T, SUFFIX)     T: CAT(carray_set, SUFFIX),
// #define CARRAY_REPR_TABLE CARRAY_ELEM_TYPES(CARRAY_REPR_CASE)
// #undef CARRAY_REPR_CASE

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
#define get(N, A, I, O) \
    _Generic(((A)[0]), \
        CARRAY_ELEM_TYPES(CARRAY__GET_CASE) \
        default: carray_get_void_ptr \
    )((N), (A), (I), (O))


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
#define get_or(N, A, I, O, F) \
    _Generic(((A)[0]), \
        CARRAY_ELEM_TYPES(CARRAY__GET_OR_CASE) \
        default: carray_get_or_void_ptr \
    )((N), (A), (I), (O), (F))

/**
 * Prints a representation of the array via printf, on the current line. Does not print a newline.
 * If `limit` is greater than 0, uses this to limit the number of displayed elements to MIN(n, limit).
 * If `message` is not null, uses this string as the output prefix instead of array element type.
 * @param n size_t: number of elements in the array
 * @param array the array to display
 * @param message char*: optional, if null prefixes output with array element type, otherwise uses this as the prefix
 * @param limit size_t: if > 0, restricts output to the first MIN(n, limit) array elements. If 0, limits to DEFAULT_LIMIT items.
 */
#define repr(N, A, M, L) \
    _Generic(((A)[0]), \
        CARRAY_ELEM_TYPES(CARRAY__REPR_CASE) \
        default: carray_repr_void_ptr \
    )((N), (A), (M), (L))


/**
 * Sets the value of array[index] to the `value` argument.
 * @param n size_t: number of elements in the array
 * @param array the array for which to set the value
 * @param index size_t: the index position of the array. If greater than n-1, returns CARRAY_ERR_INDEX_OUT_OF_RANGE
 * @param value the value to set at array[index]
 * @return CArrayError: CARRAY_OK on success, or error code on failure.
 */
#define set(N, A, I, V) \
    _Generic(((A)[0]), \
        CARRAY_ELEM_TYPES(CARRAY__SET_CASE) \
        default: carray_set_void_ptr \
    )((N), (A), (I), (V))