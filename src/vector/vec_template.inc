//
// Created by Rob Ross on 2/23/26.
//


// -----------------------------------
//
// DO NOT INCLUDE THIS FILE.
//
// this file is intended to ONLY be included by vector_types.h. It generates implementations of a Vector for
// various element types for use by generic programming, such as VectorInt, VectorChar, etc.
//
// -----------------------------------


// ReSharper disable once CppMissingIncludeGuard
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "vec_template.h"

// todo remove these ifndefs
// temp during development so IDE editor has a concrete type
// #ifndef VEC_NAME
// #   define VEC_NAME VectorInt
// #endif
// #ifndef VEC_T
// #   define VEC_T int
// #endif


// --------------------------------------------------------
// Generic Vector type (e.g., VectorInt, VectorChar, etc.)
// --------------------------------------------------------

typedef struct VEC_NAME {
    size_t length;  // current number of elements in the vector
    size_t capacity;  // total number of elements the buffer can hold
    size_t element_size; // size in bytes of a single element in the Vector
    char const * vector_type_name; //perhaps this should be an enum, we have identified 46 types so far.
    VEC_T *v;
} VEC_NAME;


#if defined(VEC_IMPLEMENTATION)
/**
 * Ensures this Vector has the capacity specified in the argument `capacity`. If the current capacity is less than the
 * argument, expands the Vector by doubling in size until the Vector capacity >= `capacity`.
 * @param v the vector to check
 * @param capacity the desired capacity size
 * @return VEC_OK on success, or an error code on failure
 */
static VecError VEC_FN(_ensure_capacity)(VEC_NAME *v, const size_t capacity) {
    if (capacity >= v->capacity) {
        // must grow buffer
        size_t new_capacity = v->capacity ? v->capacity * 2 : 1;
        do {
            new_capacity *= 2;  // keep doubling capacity until we can accomodate argument capacity
        } while (new_capacity < capacity);
        if (v->capacity > SIZE_MAX / 2 || new_capacity > SIZE_MAX / sizeof(VEC_T)) {
            return VEC_ERR_OVERFLOW;
        }
        VEC_T *re_ptr = realloc(v->v, new_capacity * sizeof(VEC_T));
        if (!re_ptr) {
            return VEC_ERR_OUT_OF_MEM;
        }
        v->v = re_ptr;
        v->capacity = new_capacity;
    }
    return VEC_OK;
}
#endif

#if !defined(VEC_IMPLEMENTATION)
/** Add the value `element` to the end of the Vector, increasing its length by 1.
 * @param v vector to append to
 * @param element element to append
 * @return VEC_OK on success, or an error code on failure
 */
VecError VEC_FN(_append)(VEC_NAME *v, VEC_T element);
#else
VecError VEC_FN(_append)(VEC_NAME *v, const VEC_T element) {
    if (!v) {
        return VEC_ERR_NULL_ARG;
    }
    const VecError result = VEC_FN(_ensure_capacity)(v, v->length);
    if (result  != VEC_OK) {
        return result;
    }
    v->v[v->length++] = element;
    return VEC_OK;
}
#endif


#if !defined(VEC_IMPLEMENTATION)
/** Adds the values in the  `elements` array to the end of the Vector, increasing its length by n.
 * @param v vector to append to
 * @param n number of elements to be appended
 * @param elements VEC_T[] elements to append
 * @return VEC_OK on success, or an error code on failure
 */
VecError VEC_FN(_append_all)(VEC_NAME *v, size_t n, const VEC_T elements[static n]);
#else
VecError VEC_FN(_append_all)(VEC_NAME *v, const size_t n, const VEC_T elements[static n]) {
    if (!v) {
        return VEC_ERR_NULL_ARG;
    }
    const int result = VEC_FN(_ensure_capacity)(v, v->length + n);
    if (result  != VEC_OK) {
        return result;
    }
    memcpy(&v->v[v->length], elements, n * sizeof(v->v[0]));
    v->length += n;
    return VEC_OK;
}
#endif



#if !defined(VEC_IMPLEMENTATION)
VecError VEC_FN(_get)(VEC_NAME *v, VEC_T *out, const size_t index);
#else
VecError VEC_FN(_get)(VEC_NAME *v, VEC_T *out, const size_t index) {
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
#endif

#if !defined(VEC_IMPLEMENTATION)
VecError VEC_FN(_get_or)(VEC_NAME *v, VEC_T *out, const size_t index, const VEC_T fallback);
#else
VecError VEC_FN(_get_or)(VEC_NAME *v, VEC_T *out, const size_t index, const VEC_T fallback) {
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
#endif

#if !defined(VEC_IMPLEMENTATION)
VecError VEC_FN(_init)(VEC_NAME *vector_out, const size_t initial_capacity);
#else
VecError VEC_FN(_init)(VEC_NAME *vector_out, const size_t initial_capacity) {
    if (!vector_out) {
        return VEC_ERR_NULL_ARG;
    }
    *vector_out = (VEC_NAME){0};
    // if (initial_capacity == 0 ) {
    //     return VEC_ERR_ZERO_CAP;
    // }
    if ( initial_capacity > SIZE_MAX / sizeof(VEC_T)) {
        return VEC_ERR_OVERFLOW; // overflow guard
    }

    vector_out->v = calloc(initial_capacity, sizeof(VEC_T));
    if ( !vector_out->v) {
        return VEC_ERR_OUT_OF_MEM;
    }
    vector_out->length = 0;
    vector_out->capacity = initial_capacity;
    vector_out->element_size = sizeof(VEC_T);
    vector_out->vector_type_name = STRINGIFY(VEC_NAME);
    return VEC_OK;
}
#endif

#if !defined(VEC_IMPLEMENTATION)
VecError VEC_FN(_insert)(VEC_NAME *v, const VEC_T element, const size_t index);
#else
VecError VEC_FN(_insert)(VEC_NAME *v, const VEC_T element, const size_t index) {
    if (!v) {
        return VEC_ERR_NULL_ARG;
    }
    if ( index > v->length ) {
        return VEC_ERR_INDEX_OUT_OF_RANGE;
    }
    const VecError result = VEC_FN(_ensure_capacity)(v, v->length);
    if (result  != VEC_OK) {
        return result;
    }
    memmove(&v->v[index + 1], &v->v[index], (v->length - index) * sizeof *v->v );
    v->length++;
    v->v[index] = element;

    return VEC_OK;
}
#endif

#if !defined(VEC_IMPLEMENTATION)
VecError VEC_FN(_pop)(VEC_NAME *v, VEC_T *out);
#else
VecError VEC_FN(_pop)(VEC_NAME *v, VEC_T *out) {
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
#endif

#if !defined(VEC_IMPLEMENTATION)
VEC_T VEC_FN(_pop_or)(VEC_NAME *v, const VEC_T fallback );
#else
VEC_T VEC_FN(_pop_or)(VEC_NAME *v, const VEC_T fallback ) {
    if (!v || v->length == 0 ) {
        return fallback;
    }
    v->length--;
    return v->v[v->length];
}
#endif

#if !defined(VEC_IMPLEMENTATION)
VEC_T VEC_FN(_pop_unsafe)(VEC_NAME *v);
#else
VEC_T VEC_FN(_pop_unsafe)(VEC_NAME *v) {
    v->length--;
    return v->v[v->length];
}
#endif

#if !defined(VEC_IMPLEMENTATION)
VecError VEC_FN(_remove)(VEC_NAME *v, const size_t index, VEC_T *out) ;
#else
VecError VEC_FN(_remove)(VEC_NAME *v, const size_t index, VEC_T *out) {
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
                (v->length - index - 1) * sizeof(VEC_T));
    }
    v->length--;
    return VEC_OK;
}
#endif




// todo remove after development.
// #undef VEC_T
// #undef VEC_NAME