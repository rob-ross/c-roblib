//
//  vector_types.c
//
// Created by Rob Ross on 2/23/26.
//

#include "vec_template.h"


const char *vec_err_str(const VecError err)
{
    switch (err) {
        case VEC_OK:
            return "ok";
        case VEC_ERR_NULL_ARG:
            return "null argument";
        case VEC_ERR_ZERO_CAP:
            return "zero capacity";
        case VEC_ERR_OVERFLOW:
            return "size overflow";
        case VEC_ERR_OUT_OF_MEM:
            return "out of memory";
        case VEC_ERR_EMPTY:
            return "empty vector";
        case VEC_ERR_INDEX_OUT_OF_RANGE:
            return "index out of range";
        default:
            return "unknown error";
    }
}

#if !defined(VEC_IMPLEMENTATION)
#   define VEC_IMPLEMENTATION
#   include "vector_types.h"
#endif
