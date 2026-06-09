//
//  array_types.c
//
// Created by Rob Ross on 2/24/26.
//

#include "array_template.h"

const char *array_err_str(const ArrayError err)
{
    switch (err) {
        case ARRAY_OK:
            return "ok";
        case ARRAY_ERR_NULL_ARG:
            return "null argument";
        case ARRAY_ERR_OUT_OF_MEM:
            return "out of memory";
        case ARRAY_ERR_EMPTY:
            return "empty array";
        case ARRAY_ERR_INDEX_OUT_OF_RANGE:
            return "index out of range";
        default:
            return "unknown error";
    }
}


#if !defined(ARRAY_IMPLEMENTATION)
#   define ARRAY_IMPLEMENTATION
#   include "array_types.h"
#endif