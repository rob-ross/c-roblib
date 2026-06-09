//
//  array_template.h
//
// Created by Rob Ross on 2/24/26.
//

#pragma once

#include "../base.h"

typedef enum ArrayError {
    ARRAY_OK = 0,
    ARRAY_ERR_NULL_ARG,
    ARRAY_ERR_OUT_OF_MEM,
    ARRAY_ERR_EMPTY,
    ARRAY_ERR_INDEX_OUT_OF_RANGE
} ArrayError;

/**
 * @param err error code returned by Array functions
 * @return human-readable string for the error code
 */
const char *array_err_str(ArrayError err);

// -------------------------------------
// Helper Macros
// -------------------------------------

#define ARRAY_FN(name) CAT( CAT(array, name), CAT(_, ARRAY_NAME))