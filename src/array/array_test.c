//
// Created by Rob Ross on 2/24/26.
//

#include "../base.h"
#include "array_types.h"

void display_array(const ArrayInt *array) {
    printf("sizeof(array)=%zu, array length=%zu, element_size=%zu, element_type_name=%s, array_type_name=%s, is_scalar=%d\n",
        sizeof(*array), array->length, array->element_size, array->element_type_name, array->array_type_name, array->is_scalar);
    fflush(stdout);
    for (int i = 0; i < array->length; i++ ) {
        printf("%i,",array->array[i]);
    }
    printf("\n");
}

// make:
// clang flag --save-temps will save state of interim file
// clang -std=c23 -Wall -Werror -o array_test.out  array_test.c --save-temps array_types.c

// clang -std=c23 -Wall -Werror -o ./out/array_test.out  array_test.c array_types.c
//
int main(int argc, char *argv[]) {
    ArrayInt *aint_p = {};
    ArrayError error = {};
    if ( (error = array_init_ArrayInt(&aint_p, 20, true) )  ) {
        printf("error initializing ArrayInt: %s\n", array_err_str(error));
        return EXIT_FAILURE;
    }
    display_array(aint_p);
    array_repr_ArrayInt(aint_p, "initialized ArrayInt");

    array_dispose_ArrayInt(&aint_p);
}
