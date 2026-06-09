//
// Created by Rob Ross on 1/28/26.
//
// Implementation of a dynamically sized array that can add new elements, select elements, and grow as needed.
// to make: clang -DVECTOR_TEST_MAIN vector.c -o vector
//
// This file serves as prototypes for the generic templates in "vec_template.inc".
// This file implements a single concrete vector of int, "VectorInt".  concrete code can be added and tested here,
// and then added to vec_template.inc in a generified format. It's much easiser to debug this concrete class
// than deal with several layers of macro indirection in the generic cases.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "vector.h"
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

/**
 * Ensures this Vector has the capacity specified in the argument `capacity`. If the current capacity is less than the
 * argument, expands the Vector by doubling in size until the Vector capacity >= `capacity`.
 * @param v the vector to check
 * @param capacity the desired capacity size
 * @return VEC_OK on success, or an error code on failure
 */
static VecError vector_ensure_capacity(VectorInt *v, const size_t capacity) {
    if ( capacity >= v->capacity ) {
        // must grow buffer
        size_t new_capacity = v->capacity ? v->capacity * 2 : 1;

        do {
            new_capacity *= 2;  // keep doubling capacity until we can accomodate argument capacity
        } while (new_capacity < capacity);

        if (v->capacity > SIZE_MAX / 2 || new_capacity > SIZE_MAX / sizeof(int)) {
            return VEC_ERR_OVERFLOW;
        }
        int *re_ptr = realloc(v->v, new_capacity * sizeof(int));
        if (!re_ptr) {
            return VEC_ERR_OUT_OF_MEM;
        }
        v->v = re_ptr;
        v->capacity = new_capacity;
    }
    return VEC_OK;
}

VecError vector_append(VectorInt *v, const int element) {
    if (!v) {
        return VEC_ERR_NULL_ARG;
    }
    const int result = vector_ensure_capacity(v, v->length);
    if (result  != VEC_OK) {
        return result;
    }
    v->v[v->length++] = element;
    return VEC_OK;
}

VecError vector_append_all(VectorInt *v, const size_t n, const int elements[static n]) {
    if (!v) {
        return VEC_ERR_NULL_ARG;
    }
    const int result = vector_ensure_capacity(v, v->length + n);
    if (result  != VEC_OK) {
        return result;
    }
    memcpy(&v->v[v->length], elements, n * sizeof(v->v[0]));
    v->length += n;
    return VEC_OK;
}

VecError vector_get(VectorInt *v, int *out, const size_t index) {
    if (!v) {
        return VEC_ERR_NULL_ARG;
    }
    if (v->length == 0 ) {
        return VEC_ERR_EMPTY;
    }
    if (  v->length - 1 < index ) {
        return VEC_ERR_INDEX_OUT_OF_RANGE;
    }
    *out = v->v[index];
    return VEC_OK;
}

VecError vector_get_or(VectorInt *v, int *out, size_t index, const int fallback) {
    if (!v) {
        return VEC_ERR_NULL_ARG;
    }
    if (v->length == 0 ) {
        return VEC_ERR_EMPTY;
    }
    if (  v->length - 1 < index ) {
        *out = fallback;
        return VEC_OK;
    }
    *out = v->v[index];
    return VEC_OK;
}

VecError vector_init (VectorInt *vector_out, const size_t initial_capacity) {
    if (!vector_out) {
        return VEC_ERR_NULL_ARG;
    }
    *vector_out = (VectorInt){0};
    // if (initial_capacity == 0 ) {
    //     return VEC_ERR_ZERO_CAP;
    // }
    if ( initial_capacity > SIZE_MAX / sizeof(int)) {
        return VEC_ERR_OVERFLOW; // overflow guard
    }

    vector_out->v = calloc(initial_capacity, sizeof(int));
    if ( !vector_out->v) {
        return VEC_ERR_OUT_OF_MEM;
    }
    vector_out->length = 0;
    vector_out->capacity = initial_capacity;
    vector_out->element_size = sizeof(int);
    vector_out->vector_type_name = "VectorInt";
    vector_out->element_type_name = "int";

    vector_out->capacity = initial_capacity;
    return VEC_OK;
}

VecError vector_insert(VectorInt *v, const int element, const size_t index) {
    if (!v) {
        return VEC_ERR_NULL_ARG;
    }
    if ( index > v->length ) {
        return VEC_ERR_INDEX_OUT_OF_RANGE;
    }
    const int result = vector_ensure_capacity(v, v->length);
    if (result  != VEC_OK) {
        return result;
    }
    memmove(&v->v[index + 1], &v->v[index], (v->length - index) * sizeof *v->v );
    v->length++;
    v->v[index] = element;

    return VEC_OK;
}

VecError vector_pop(VectorInt *v, int *out) {
    if (!v || !out) {
        return VEC_ERR_NULL_ARG;
    }
    if (v->length == 0 ) {
        return VEC_ERR_EMPTY;
    }
    *out = v->v[v->length - 1];
    v->length--;
    return VEC_OK;
}

int vector_pop_or(VectorInt *v, int fallback ) {
    if (!v || v->length == 0 ) {
        return fallback;
    }
    v->length--;
    return v->v[v->length];
}

int vector_pop_unsafe(VectorInt *v) {
    v->length--;
    return v->v[v->length];
}

VecError vector_remove(VectorInt *v, const size_t index, int *out) {
    if (!v || !out) {
        return VEC_ERR_NULL_ARG;
    }
    if (v->length == 0 ) {
        return VEC_ERR_EMPTY;
    }
    if (  v->length - 1 < index ) {
        return VEC_ERR_INDEX_OUT_OF_RANGE;
    }
    *out = v->v[index];
    // copy v[i + 1] to v[i] for all i > index
    if (index + 1 < v->length) {
        memmove(&v->v[index], &v->v[index + 1],
                (v->length - index - 1) * sizeof v->v[0]);
    }
    v->length--;
    return VEC_OK;
}




#ifdef VECTOR_TEST_MAIN

void display_vector(VectorInt v) {
    printf("vector length=%zu, capacity=%zu, element_size=%zu, element_type_name=%s, vector_type_name=%s [", v.length, v.capacity, v.element_size, v.element_type_name, v.vector_type_name);
    for (int i = 0; i < v.length; i++ ) {
        printf("%i,",v.v[i]);
    }
    printf("]\n");
}

//print the error and return true if an error occurred
bool display_error(VecError error) {
    if ( error) {
        printf("error=%s\n", vec_err_str(error));
    }
    return error != VEC_OK;
}

void handle_error(VecError error) {
    if (display_error(error)) {
        exit(error);
    }
}


void t1(void) {
    // quick test of vector
    VectorInt vec;
    VecError err = vector_init(&vec, 2);

    for (int i = 0; i < 10; i++) {
        printf("err=%s ", vec_err_str(err));
        display_vector(vec);
        err = vector_append(&vec, i);
    }
    printf("err=%s ", vec_err_str(err));
    display_vector(vec);
    printf("\nRemoving;");

    for (int i = 0; i < 10; i++) {
        printf("err=%s ", vec_err_str(err));
        display_vector(vec);
        int out;
        err = vector_remove(&vec, 1,  &out);  // removing first element each time
    }
    printf("err=%s. ", vec_err_str(err));
    display_vector(vec);
}

int t2(void) {
    VectorInt vec;
    VecError err = vector_init(&vec, 1);
    handle_error(err);
    display_vector(vec);

    err = vector_append(&vec, 42);
    handle_error(err);
    display_vector(vec);

    err = vector_append(&vec, 67);
    handle_error(err);
    display_vector(vec);

    err = vector_append_all(&vec, 5, (int[]){1,2,3,4,5});
    handle_error(err);
    display_vector(vec);

    return EXIT_SUCCESS;
}

// make:
// clang -std=c23 -DVECTOR_TEST_MAIN -o vector.c.out vector.c

int main(void) {
    return t2();
}

#endif
