//
//  carray_template.h
//
// Created by Rob Ross on 2/24/26.
//

#pragma once

#include "../base.h"


typedef enum CArrayError: unsigned char {
    CARRAY_OK = 0,
    CARRAY_ERR_NULL_ARG,
    CARRAY_ERR_OUT_OF_MEM,
    CARRAY_ERR_EMPTY,
    CARRAY_ERR_INDEX_OUT_OF_RANGE,
    CARRAY_ERR_UNNOWN_ERR,
} CArrayError;

typedef enum CTypeEnum: unsigned char {
    CTYPE_BOOL,      CTYPE_BOOL_PTR,      CTYPE_BOOL_CPTR,
    CTYPE_CHAR,      CTYPE_CHAR_PTR,      CTYPE_CHAR_CPTR,
    CTYPE_UCHAR,     CTYPE_UCHAR_PTR,     CTYPE_UCHAR_CPTR,
    CTYPE_SCHAR,     CTYPE_SCHAR_PTR,     CTYPE_SCHAR_CPTR,
    CTYPE_SHORT,     CTYPE_SHORT_PTR,     CTYPE_SHORT_CPTR,
    CTYPE_USHORT,    CTYPE_USHORT_PTR,    CTYPE_USHORT_CPTR,
    CTYPE_INT,       CTYPE_INT_PTR,       CTYPE_INT_CPTR,
    CTYPE_UINT,      CTYPE_UINT_PTR,      CTYPE_UINT_CPTR,
    CTYPE_LONG,      CTYPE_LONG_PTR,      CTYPE_LONG_CPTR,
    CTYPE_ULONG,     CTYPE_ULONG_PTR,     CTYPE_ULONG_CPTR,
    CTYPE_LLONG,     CTYPE_LLONG_PTR,     CTYPE_LLONG_CPTR,
    CTYPE_ULLONG,    CTYPE_ULLONG_PTR,    CTYPE_ULLONG_CPTR,
    CTYPE_FLOAT,     CTYPE_FLOAT_PTR,     CTYPE_FLOAT_CPTR,
    CTYPE_DOUBLE,    CTYPE_DOUBLE_PTR,    CTYPE_DOUBLE_CPTR,
    CTYPE_LDOUBLE,   CTYPE_LDOUBLE_PTR,   CTYPE_LDOUBLE_CPTR,
    CTYPE_FCOMPLEX,  CTYPE_FCOMPLEX_PTR,  CTYPE_FCOMPLEX_CPTR,
    CTYPE_DCOMPLEX,  CTYPE_DCOMPLEX_PTR,  CTYPE_DCOMPLEX_CPTR,
    CTYPE_LDCOMPLEX, CTYPE_LDCOMPLEX_PTR, CTYPE_LDCOMPLEX_CPTR,
    CTYPE_VOID_PTR,  CTYPE_VOID_CPTR,
} CTypeEnum;

/**
 * Returns a string representing the enumeration constant. This string is a literal and should not be freed.
 * @param err error code returned by Array functions
 * @return human-readable string for the error code
 */
const char *carray_err_str(CArrayError err);

/**
 * Return a string representing the enumeration constant. This string is a literal and should not be freed.
 * @param ctype a CTypeEnum enumeration constant
 * @return human-readable string for the constant
 */
const char *ctype_repr_str( CTypeEnum  ctype);

#define DEFAULT_LIMIT 10

// -------------------------------------
// Helper Macros
// -------------------------------------

#define CARRAY_FN(name) CAT3(carray, name, CARRAY_NAME)


