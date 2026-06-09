#pragma once

#include <stddef.h>
#include "vec_template.h"

// element type is either scalar/primative or ptr type.

typedef struct VectorInt {
    size_t length;  // current number of elements in the vector
    size_t capacity;  // total number of elements the buffer can hold
    size_t element_size; // size in bytes of a single element
    char* element_type_name;
    char* vector_type_name; // eventually this will be an enum, we have identified 46 types so far.
    int *v;
} VectorInt;


/** Adds the value `element` to the end of the Vector, increasing its length by 1.
 * @param v vector to append to
 * @param element element to append
 * @return VEC_OK on success, or an error code on failure
 */
VecError vector_append(VectorInt *v, int element);


/** Adds the values in the  `elements` array to the end of the Vector, increasing its length by n.
 * @param v vector to append to
 * @param n number of elements to be appended
 * @param elements VEC_T[] elements to append
 * @return VEC_OK on success, or an error code on failure
 */
VecError vector_append_all(VectorInt *v, size_t n, const int elements[static n]);
/**
 * Returns the value at index
 * @param v vector to retreive value from
 * @param out destination for the returned value
 * @param index the index position in the vector, from 0 to length-1
 * @return the value at the index given by the `index` argument. If `index` is invalid returns
 * VEC_ERR_INDEX_OUT_OF_RANGE
 */
VecError vector_get(VectorInt *v, int *out, size_t index);

/**
 * Returns in `out` the value at index or the fallback value if index is out of range of 0 to length-1.
 * @param v vector to retreive value from
 * @param out destination for the returned value
 * @param index the index position in the vector, from 0 to length-1
 * @param fallback if index is not in range of 0 to length-1, returns this value instead.
 * @return the value at the index given by the `index` argument, or `fallback` if index is out of range.
 */
VecError vector_get_or(VectorInt *v, int *out, size_t index, int fallback);

/**
 * @param vector_out destination vector to initialize (zeroed on error)
 * @param initial_capacity number of int elements to pre-allocate space for
 * @return VEC_OK on success, or an error code on failure (out is zeroed on error). Use vec_err_str to get a
 * human-readable error message.
 */
VecError vector_init(VectorInt *vector_out, size_t initial_capacity);


/**
 * Insert the value `element` in the Vector at index position `index`. This causes the value of Vector[index] to contain
 * the new value `element`, increases the length of the Vector by 1, and moves every existing value from the
 * original Vector[index] to the right by 1 place.
 * @param v vector to insert into
 * @param element element to insert
 * @param index the index position in the vector at which to insert `i`, from 0 to length-1
 * @return VEC_OK on success, or an error code on failure
 */
VecError vector_insert(VectorInt *v, int element, size_t index);


/**
 * Removes the last element and returns it in `out`. After this call, the length of the vector is reduced by 1.
 * @param v vector to pop from
 * @param out destination for the popped value
 * @return VEC_OK on success, or an error code on failure
 */
VecError vector_pop(VectorInt *v, int *out);

/**
 * @param v vector to pop from
 * @param fallback value to return when v is NULL or empty
 * @return the popped value, or fallback if no value was available
 */
int vector_pop_or(VectorInt *v, int fallback);

/**
 * For internal use. Undefined on NULL/empty.
 * @param v vector to pop from
 * @return the popped value
 */
int vector_pop_unsafe(VectorInt *v);

/**
 * Remove the element at `index` and return the value in `out`. Values > index move down by one to fill the hole.
 * The length of the Vector is reduced by 1.
 * @param v vector to remove from
 * @param index the index position in the vector from which to remove, from 0 to length-1
 * @param out the value of the removed element is written to `out`
 * @return VEC_OK on success, or an error code on failure
 */
VecError vector_remove(VectorInt *v, size_t index, int *out);

