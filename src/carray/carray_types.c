//
//  carray_types.c
//
// Created by Rob Ross on 2/24/26.
//

#include "carray_template.h"

const char *carray_err_str(const CArrayError err) {
    switch (err) {
        case CARRAY_OK:
            return "ok";
        case CARRAY_ERR_NULL_ARG:
            return "null argument";
        case CARRAY_ERR_OUT_OF_MEM:
            return "out of memory";
        case CARRAY_ERR_EMPTY:
            return "empty array";
        case CARRAY_ERR_INDEX_OUT_OF_RANGE:
            return "index out of range";
        case CARRAY_ERR_UNNOWN_ERR:
        default:
            return "unknown error";
    }
}

const char *ctype_repr_str( const CTypeEnum ctype ) {
    switch( ctype) {
        case CTYPE_BOOL:            return "bool";
        case CTYPE_BOOL_PTR:        return "bool*";
        case CTYPE_BOOL_CPTR:       return "bool const*";
        case CTYPE_CHAR:            return "char";
        case CTYPE_CHAR_PTR:        return "char*";
        case CTYPE_CHAR_CPTR:       return "char const*";
        case CTYPE_UCHAR:           return "unsigned char";
        case CTYPE_UCHAR_PTR:       return "unsigned char*";
        case CTYPE_UCHAR_CPTR:      return "unsigned char const*";
        case CTYPE_SCHAR:           return "signed char";
        case CTYPE_SCHAR_PTR:       return "signed char*";
        case CTYPE_SCHAR_CPTR:      return "signed char const*";
        case CTYPE_SHORT:           return "short";
        case CTYPE_SHORT_PTR:       return "short*";
        case CTYPE_SHORT_CPTR:      return "short const*";
        case CTYPE_USHORT:          return "unsigned short";
        case CTYPE_USHORT_PTR:      return "unsigned short*";
        case CTYPE_USHORT_CPTR:     return "unsigned short const*";
        case CTYPE_INT:             return "int";
        case CTYPE_INT_PTR:         return "int*";
        case CTYPE_INT_CPTR:        return "int const*";
        case CTYPE_UINT:            return "unsigned int";
        case CTYPE_UINT_PTR:        return "unsigned int*";
        case CTYPE_UINT_CPTR:       return "unsigned int const*";
        case CTYPE_LONG:            return "long";
        case CTYPE_LONG_PTR:        return "long*";
        case CTYPE_LONG_CPTR:       return "long const*";
        case CTYPE_ULONG:           return "unsigned long";
        case CTYPE_ULONG_PTR:       return "unsigned long*";
        case CTYPE_ULONG_CPTR:      return "unsigned long const*";
        case CTYPE_LLONG:           return "long long";
        case CTYPE_LLONG_PTR:       return "long long*";
        case CTYPE_LLONG_CPTR:      return "long long const*";
        case CTYPE_ULLONG:          return "unsigned long long";
        case CTYPE_ULLONG_PTR:      return "unsigned long long*";
        case CTYPE_ULLONG_CPTR:     return "unsigned long long const*";
        case CTYPE_FLOAT:           return "float";
        case CTYPE_FLOAT_PTR:       return "float*";
        case CTYPE_FLOAT_CPTR:      return "float const*";
        case CTYPE_DOUBLE:          return "double";
        case CTYPE_DOUBLE_PTR:      return "double*";
        case CTYPE_DOUBLE_CPTR:     return "double const*";
        case CTYPE_LDOUBLE:         return "long double";
        case CTYPE_LDOUBLE_PTR:     return "long double*";
        case CTYPE_LDOUBLE_CPTR:    return "long double const*";
        case CTYPE_FCOMPLEX:        return "float _Complex";
        case CTYPE_FCOMPLEX_PTR:    return "float _Complex*";
        case CTYPE_FCOMPLEX_CPTR:   return "float _Complex const*";
        case CTYPE_DCOMPLEX:        return "double _Complex";
        case CTYPE_DCOMPLEX_PTR:    return "double _Complex*";
        case CTYPE_DCOMPLEX_CPTR:   return "double _Complex const*";
        case CTYPE_LDCOMPLEX:       return "long double _Complex";
        case CTYPE_LDCOMPLEX_PTR:   return "long double _Complex*";
        case CTYPE_LDCOMPLEX_CPTR:  return "long double _Complex const*";
        case CTYPE_VOID_PTR:        return "void*";
        case CTYPE_VOID_CPTR:       return "void const*";
        default:                    return "unknown type";
    }
}


#if !defined(CARRAY_IMPLEMENTATION)
#   define CARRAY_IMPLEMENTATION
#   include "carray_types.h"
#endif