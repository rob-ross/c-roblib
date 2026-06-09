//
//  carray.c
//
// Prototype code for generic C array methods
//
// Created by Rob Ross on 2/24/26.
//

#include <stdio.h>

#include "../base.h"
#include "carray.h"
#include "../dev_utils.h"


#define DEFAULT_LIMIT 10
const char *carray_err_str(const CArrayError err) {
    switch (err) {
        case CARRAY_OK:
            return "ok";
        case CARRAY_ERR_NULL_ARG:
            return "null argument";
        case CARRAY_ERR_OUT_OF_MEM:
            return "out of memory";
        case CARRAY_ERR_EMPTY:
            return "empty array";
        case CARRAY_ERR_INDEX_OUT_OF_RANGE:
            return "index out of range";
        default:
            return "unknown error";
    }

}

CArrayError carray_get_int(const size_t n, const int array[static n], const size_t index, int *out) {
    if (!array) {
        return CARRAY_ERR_NULL_ARG;
    }
    if ( n == 0) {
        return CARRAY_ERR_EMPTY;
    }
    if (  index >= n ) {
        return CARRAY_ERR_INDEX_OUT_OF_RANGE;
    }
    *out = array[index];
    return CARRAY_OK;
}

// ReSharper disable once CppDFAConstantFunctionResult
CArrayError carray_get_or_int(const size_t n, const int array[static n], const size_t index, int *out, const int fallback) {
    CArrayError error = carray_get_int(n, array, index, out);
    if (!error) {
        return CARRAY_OK;
    }
    *out = fallback;
    return CARRAY_OK;
}

void carray_repr_int(const size_t n, const int array[static n], char const *message, const size_t limit) {
    const char *msg = message ? message : "int";
    size_t num_to_display = n;
    if (limit == 0) {
        num_to_display = MIN(DEFAULT_LIMIT, n);
    } else {
        num_to_display = MIN(limit, n);
    }
    printf("(%s[%zu]){", msg, n);
    if (num_to_display > 0 ) {
        printf(" %d", array[0]);
    }
    for (size_t index = 1; index < num_to_display; ++index) {
        printf(", %d", array[index]);
    }
    if (num_to_display < n) {
        printf(", ...");
    }
    printf(" }");
}

CArrayError carray_set_int(const size_t n, int array[static n], const size_t index, const int value) {
    if (!array) {
        return CARRAY_ERR_NULL_ARG;
    }
    if ( n == 0) {
        return CARRAY_ERR_EMPTY;
    }
    if (  index >= n ) {
        return CARRAY_ERR_INDEX_OUT_OF_RANGE;
    }
    array[index] = value;
    return CARRAY_OK;
}

void t1() {
    int array[10] = {1,2,3,4,5,6,7,8,9,10};
    carray_repr_int(10, array, nullptr, 0);
    printf("\nlimit = 5: ");
    carray_repr_int(10, array, nullptr, 5);
    printf("\nlimit = 50: ");
    carray_repr_int(10, array, nullptr, 50);
    printf("\ncustom message:  ");
    carray_repr_int(10, array, "myarray", 5);
}

void test_set_int(void) {
    // first non-const array
    int array[10] = {1,2,3,4,5,6,7,8,9,10};
    print("");
    carray_repr_int(10, array, nullptr, 0);

    CArrayError result;
    result = carray_set_int(10, array, 4, 9999);
    print("\nset(4, 9999)  error = %s\n", carray_err_str(result));
    carray_repr_int(10, array, nullptr, 0);

    //now a const int array
    int const array2[10] = {1,2,3,4,5,6,7,8,9,10};
    print("");
    carray_repr_int(10, array2, nullptr, 0);

    int *cast_array = (void*)array2;
    result = carray_set_int(10, cast_array, 5, 676767);
    print("\nset(5, 676767)  error = %s\n", carray_err_str(result));
    carray_repr_int(10, array2, nullptr, 0);
    print("");
}


// make
// clang -std=c23 -Wall -Werror -o ./out/carray.c.out carray.c

int main(int argc, char *argv[]) {
    t1();
    test_set_int();
}

