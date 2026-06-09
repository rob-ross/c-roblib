//
// Created by Rob Ross on 2/23/26.
//

#include <stdio.h>
#include "../base.h"
#include "vector_types.h"


void vector_repr(VectorInt const vint, char const *message) {
    printf(
        "%s. length=%zu, capacity=%zu, element size=%zu, type name=%s\n",
        message, vint.length, vint.capacity, vint.element_size, vint.type_name
    );
    size_t num_to_display = MIN(10, vint.length);
    printf("(int[%zu]){", vint.length);
    if (num_to_display > 0 ) {
        printf(" %d", vint.v[0]);
    }
    for (size_t index = 1; index < num_to_display; ++index) {
        printf(", %d", vint.v[index]);
    }
    if (num_to_display < vint.length) {
        printf(", ...");
    }
    printf(" }\n");
}


//make:
// clang -std=c23 -o vector_test.out vector_test.c vector_types.c
int main(int argc, char *argv[]) {
    VectorInt vint = {};
    VecError error = {};
    if ( (error = vec_init_VectorInt(&vint, 0) )  ) {
        printf("error initializing VectorInt: %s\n", vec_err_str(error));
        return EXIT_FAILURE;
    }
    vector_repr(vint, "initialized VectorInt.");

    if ( (error = vec_append_VectorInt(&vint, 42) )  ) {
        printf("error adding to VectorInt: %s\n", vec_err_str(error));
        return EXIT_FAILURE;
    }
    vector_repr(vint, "after adding 42");

    if ( (error = vec_append_VectorInt(&vint, 67) )  ) {
        printf("error adding to VectorInt: %s\n", vec_err_str(error));
        return EXIT_FAILURE;
    }
    vector_repr(vint, "after adding 67");

    error = vec_append_all_VectorInt(&vint, 10, (int[]){1,2,3,4,5,6,7,8,9,10});
    if (error) {
        printf("error adding_all {1,2,3,4,5,6,7,8,9,10} to VectorInt: %s\n", vec_err_str(error));
    }
    vector_repr(vint, "after adding {1,2,3,4,5,6,7,8,9,10}");
}
