//
// Created by Rob Ross on 2/24/26.
//

#pragma once
#include <stddef.h>
#include "../base.h"

typedef enum ArrayError {
    ARRAY_OK = 0,
    ARRAY_ERR_NULL_ARG,
    ARRAY_ERR_OUT_OF_MEM,
    ARRAY_ERR_EMPTY,
    ARRAY_ERR_INDEX_OUT_OF_RANGE
} ArrayError;

typedef struct ArrayInt {
    size_t length;  // current number of elements in the vector
    size_t element_size; // size in bytes of a single element in the Array
    char* element_type_name;   //e.g., int, char
    char* array_type_name;     // e.g. ArrayInt, ArrayChar
    bool is_scalar; // true if element type is scalar, false otherwise, e.g., pointer types.
    int array[];  // can we use flexible arrays here, since array sizes are fixed at creation?
} ArrayInt;


ArrayError array_as_array(ArrayInt * array_p, int out_array[static array_p->length]);

ArrayError array_dispose(ArrayInt **array_p);

ArrayError array_get(ArrayInt *array_p, size_t index, int *element_out) ;

ArrayError array_init(ArrayInt **array_out, size_t length, bool is_scalar);

ArrayError array_set(ArrayInt * array_p, size_t index, int element);

ArrayError array_set_all(ArrayInt * array_p, const size_t index, const size_t n, const int elements[static n]);

