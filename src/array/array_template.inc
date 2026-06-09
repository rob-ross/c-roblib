//
//  array_template.inc
//
// Created by Rob Ross on 2/24/26.
//


// -----------------------------------
//
// DO NOT INCLUDE THIS FILE.
//
// this file is intended to ONLY be included by array_types.h and array_types.c.
// It generates implementations of an Array for
// various element types for use by generic programming, such as array_int, array_char, etc.
//
// -----------------------------------


// ReSharper disable once CppMissingIncludeGuard
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "../base.h"
#include "array_template.h"

// todo remove these ifndefs
// temp during development so IDE editor has a concrete type
#ifndef ARRAY_NAME
#   define ARRAY_NAME ArrayInt
#endif
#ifndef ARRAY_T
#   define ARRAY_T int
#endif

// --------------------------------------------------------
// Generic Vector type (e.g., ArrayInt, ArrayChar, etc.)
// --------------------------------------------------------
typedef struct ARRAY_NAME {
    size_t length;  // current number of elements in the vector
    // todo element_size doesn't need to be size_t. What the max num bytes for a quantity? 16? 32?
    // and we're talking standard C data type sizes. So an uint8_t would be fine.
    // Even an unsigned _BitInt(5) gives range 0-31.
    size_t element_size; // size in bytes of a single element in the Array
    //todo for these strings, we can replace with enums and save 14 bytes, since we only need a single byte for each
    char* element_type_name;   //e.g., int, char
    char* array_type_name;     // e.g. ArrayInt, ArrayChar
    // is_scalar can be a single bit field.
    bool is_scalar; // true if element type is scalar, false otherwise, e.g., pointer types.
    ARRAY_T array[];  // can we use flexible arrays here, since array sizes are fixed at creation?
} ARRAY_NAME;



#if !defined(ARRAY_IMPLEMENTATION)
ArrayError ARRAY_FN(_as_array)(ARRAY_NAME * array_p, ARRAY_T out_array[static array_p->length]);
#else
ArrayError ARRAY_FN(_as_array)(ARRAY_NAME * array_p, ARRAY_T out_array[static array_p->length]) {
    if (!array_p || !out_array) {
        return ARRAY_ERR_NULL_ARG;
    }
    memcpy((void *)out_array, array_p->array, array_p->length);

    return ARRAY_OK;
}
#endif


#if !defined(ARRAY_IMPLEMENTATION)
ArrayError ARRAY_FN(_dispose)(ARRAY_NAME **array_p);
#else
ArrayError ARRAY_FN(_dispose)(ARRAY_NAME **array_p) {
    if (!*array_p) {
        return ARRAY_ERR_NULL_ARG;
    }
    free(*array_p);
    *array_p = nullptr;  // to prevent dangling pointers
    return ARRAY_OK;
}
#endif



#if !defined(ARRAY_IMPLEMENTATION)
ArrayError ARRAY_FN(_get)(ARRAY_NAME *array_p, const size_t index, ARRAY_T *element_out);
#else
ArrayError ARRAY_FN(_get)(ARRAY_NAME *array_p, const size_t index, ARRAY_T *element_out) {
    if (!array_p) {
        return ARRAY_ERR_NULL_ARG;
    }
    if (index > array_p->length - 1 ) {
        return ARRAY_ERR_INDEX_OUT_OF_RANGE;
    }
    *element_out = array_p->array[index];
    return ARRAY_OK;
}
#endif



#if !defined(ARRAY_IMPLEMENTATION)
ArrayError ARRAY_FN(_init)(ARRAY_NAME **array_out, const size_t length, bool is_scalar);
#else
ArrayError ARRAY_FN(_init)(ARRAY_NAME **array_out, const size_t length, bool is_scalar){
    // if (!*array_out) {
    //     return ARRAY_ERR_NULL_ARG;
    // }
    printf("sizeof(" STRINGIFY(ARRAY_NAME) "):%zu, sizeof(" STRINGIFY(ARRAY_T) "[%zu]): %zu\n", sizeof(ARRAY_NAME), length, sizeof(ARRAY_T[length]));
    *array_out = malloc(sizeof(ARRAY_NAME) + sizeof(ARRAY_T [length]));
    if (!*array_out) {
        return ARRAY_ERR_OUT_OF_MEM;
    }
    **array_out = (ARRAY_NAME){.length = length, .element_size = sizeof(ARRAY_T), .element_type_name = STRINGIFY(ARRAY_T),
        .array_type_name = STRINGIFY(ARRAY_NAME), .is_scalar = is_scalar};
    return ARRAY_OK;
}
#endif

#if !defined(ARRAY_IMPLEMENTATION)
void ARRAY_FN(_repr)(const ARRAY_NAME *array_p, char const *message) ;
#else
void ARRAY_FN(_repr)(const ARRAY_NAME *array_p, char const *message) {
    printf(
        "%s. length=%zu, element size=%zu, element name=%s, array type name=%s, is_scalar=%d\n",
        message, array_p->length, array_p->element_size, array_p->element_type_name,
        array_p->array_type_name, array_p->is_scalar
    );
    size_t num_to_display = MIN(10, array_p->length);
    printf("("STRINGIFY(ARRAY_T)"[%zu]){", array_p->length);
    if (num_to_display > 0 ) {
        printf(" %d", array_p->array[0]);
    }
    for (size_t index = 1; index < num_to_display; ++index) {
        printf(", %d", array_p->array[index]);
    }
    if (num_to_display < array_p->length) {
        printf(", ...");
    }
    printf(" }\n");
}
#endif

#if !defined(ARRAY_IMPLEMENTATION)
ArrayError ARRAY_FN(_set)(ARRAY_NAME * array_p, const size_t index, const ARRAY_T element) ;
#else
ArrayError ARRAY_FN(_set)(ARRAY_NAME * array_p, const size_t index, const ARRAY_T element) {
    if (!array_p) {
        return ARRAY_ERR_NULL_ARG;
    }
    if (index > array_p->length - 1 ) {
        return ARRAY_ERR_INDEX_OUT_OF_RANGE;
    }
    array_p->array[index] = element;

    return ARRAY_OK;
}
#endif


#if !defined(ARRAY_IMPLEMENTATION)
ArrayError ARRAY_FN(_set_all)(ARRAY_NAME * array_p, const size_t index, const size_t n, const ARRAY_T elements[static n]);
#else
ArrayError ARRAY_FN(_set_all)(ARRAY_NAME * array_p, const size_t index, const size_t n, const ARRAY_T elements[static n]) {
    if (!array_p) {
        return ARRAY_ERR_NULL_ARG;
    }
    if (index > array_p->length - 1 ) {
        return ARRAY_ERR_INDEX_OUT_OF_RANGE;
    }
    if (array_p->length) {  // nothing to copy if array length is 0
        size_t last_index = MIN(array_p->length - 1, index + n - 1);
        memcpy(&array_p->array[index], elements, sizeof(array_p->array[0]) * ( last_index - index + 1) );
    }
    return ARRAY_OK;
}
#endif





// todo remove after development.
#undef ARRAY_T
#undef ARRAY_NAME