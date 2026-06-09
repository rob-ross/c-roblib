//
// Created by Rob Ross on 2/23/26.
//

#pragma once

#include "../base.h"

typedef enum VecError {
    VEC_OK = 0,
    VEC_ERR_NULL_ARG,
    VEC_ERR_ZERO_CAP,
    VEC_ERR_OVERFLOW,
    VEC_ERR_OUT_OF_MEM,
    VEC_ERR_EMPTY,
    VEC_ERR_INDEX_OUT_OF_RANGE
} VecError;

/**
 * @param err error code returned by Vector functions
 * @return human-readable string for the error code
 */
const char *vec_err_str(VecError err);

// -------------------------------------
// Helper Macros
// -------------------------------------

#define VEC_FN(name)  CAT( CAT(vec, name), CAT(_, VEC_NAME))
