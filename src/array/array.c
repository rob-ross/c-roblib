//
// Created by Rob Ross on 2/24/26.
//

#include "array.h"

#include <stdio.h>
#include <stdlib.h>

#include "../carray/carray.h"

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

ArrayError array_as_array(ArrayInt * array_p, int out_array[static array_p->length]) {
    if (!array_p || !out_array) {
        return ARRAY_ERR_NULL_ARG;
    }
    memcpy(out_array, array_p->array, sizeof(array_p->array[0]) * array_p->length);

    return ARRAY_OK;
}

ArrayError array_dispose(ArrayInt **array_p) {
    if (!*array_p) {
        return ARRAY_ERR_NULL_ARG;
    }
    free(*array_p);
    *array_p = nullptr;  // to prevent dangling pointers
    return ARRAY_OK;
}

ArrayError array_get(ArrayInt *array_p, const size_t index, int *element_out) {
    if (!array_p) {
        return ARRAY_ERR_NULL_ARG;
    }
    if (index > array_p->length - 1 ) {
        return ARRAY_ERR_INDEX_OUT_OF_RANGE;
    }
    *element_out = array_p->array[index];
    return ARRAY_OK;
}


ArrayError array_init(ArrayInt **array_out, const size_t length, bool is_scalar){
    if (!array_out) {
        return ARRAY_ERR_NULL_ARG;
    }
    *array_out = malloc(sizeof(ArrayInt) + sizeof(int [length]));
    if (!*array_out) {
        return ARRAY_ERR_OUT_OF_MEM;
    }
    **array_out = (ArrayInt){.length = length, .element_size = sizeof(int), .element_type_name = "int", 
        .array_type_name = "ArrayInt", .is_scalar = is_scalar};
    return ARRAY_OK;
}

void array_repr(const ArrayInt *array_p, char const *message) {
    printf(
        "%s. length=%zu, element size=%zu, element name=%s array type name=%s, is_scalar=%d\n",
        message, array_p->length, array_p->element_size, array_p->element_type_name,
        array_p->array_type_name, array_p->is_scalar
    );
    size_t num_to_display = MIN(10, array_p->length);
    printf("(int[%zu]){", array_p->length);
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

ArrayError array_set(ArrayInt * array_p, const size_t index, const int element) {
    if (!array_p) {
        return ARRAY_ERR_NULL_ARG;
    }
    if (index > array_p->length - 1 ) {
        return ARRAY_ERR_INDEX_OUT_OF_RANGE;
    }
    array_p->array[index] = element;

    return ARRAY_OK;
}

ArrayError array_set_all(ArrayInt * array_p, const size_t index, const size_t n, const int elements[static n]) {
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


//print the error and return true if an error occurred
bool display_error(const ArrayError error) {
    if ( error) {
        printf("error=%s\n", array_err_str(error));
    }
    return error != ARRAY_OK;
}

void handle_error(const ArrayError error) {
    if (display_error(error)) {
        exit(error);
    }
}

void display_array(const ArrayInt *array) {
    printf("array length=%zu, element_size=%zu, element_type_name=%s, array_type_name=%s, is_scalar=%d [",
        array->length, array->element_size, array->element_type_name, array->array_type_name, array->is_scalar);
    for (int i = 0; i < array->length; i++ ) {
        printf("%i,",array->array[i]);
    }
    printf("]\n");
}



//make:
// clang -std=c23 -o ./out/array.c.out array.c ../dev_utils.c
int main(int argc, char *argv[]) {
    ArrayInt *array_p;
    ArrayError error = array_init(&array_p, 10, true);
    handle_error(error);
    display_array(array_p);

    array_set(array_p, 0, 42); // set index 0 to 42
    display_array(array_p);

    int element = {};
    error = array_get(array_p, 10, &element);  // index out of range
    // handle_error(error);

    error = array_get(array_p, 9, &element);
    printf("array_get(9) = %d\n", element);

    error = array_get(array_p, 0, &element);
    printf("array_get(0) = %d\n", element);

    printf("array_set_all( 5, 6,(int[]){1,2,3,4,5,6})\n");
    error = array_set_all(array_p, 1, 6,(int[]){1,2,3,4,5,6});
    handle_error(error);
    display_array(array_p);

    int out_array[array_p->length] = {};
    error = array_as_array(array_p, out_array);
    handle_error(error);

    carray_repr_int(array_p->length, out_array, "out_array", 0);
    printf("\n");

    // array_repr(array_p, "out_array: ");



    error = array_dispose(&array_p);
    handle_error(error);
    // display_array(array_p); // should crash now.
}
