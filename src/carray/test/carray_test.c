//
// Created by Rob Ross on 2/24/26.
//

#include <stdio.h>

#include "../carray_template.inc"
#include "../../dev_utils.h"

#import "../carray_types.h"


void show_error(CArrayError error) {
    if (error == CARRAY_OK) {
        return;
    }
    print("error = %s", carray_err_str(error));
}

void t1() {
    printf("t1() repr:\n");
    int array[10] = {1,2,3,4,5,6,7,8,9,10};

    carray_repr_int(10, array, nullptr, 0);
    printf("\nlimit = 5: ");
    carray_repr_int(10, array, nullptr, 5);
    printf("\nlimit = 50: ");
    carray_repr_int(10, array, nullptr, 50);
    printf("\ncustom message:  ");
    carray_repr_int(10, array, "myarray", 5);
    printf("\nsetting array[5] to 555\n");
    array[5] = 555; //allowed
    carray_repr_int(10, array, "myarray", 6);

    printf("\n");
}

void t2() {
    printf("t2() get:\n");
    int const array[10] = {1,2,3,4,5,6,7,8,9,10};

    // array[0] = 315; //should not be allowed
    carray_repr_int(10, array, nullptr, 0);
    printf("\n");
    int result = 0;

    CArrayError error = carray_get_int(10, array, 3, &result);
    show_error(error);
    print("get(3) = %d", result);
    error = carray_get_int(10, array, 20, &result);
    show_error(error);
    print("get(20) = %d", result);
    error = carray_get_int(0, array, 20, &result);
    show_error(error);
    print("get(20) = %d", result);

    error = get(10, array, 7, &result);
    show_error(error);
    print("get(7) = %d", result);
}

void t3() {
    printf("\nt3() test _set:\n");
    int const array[10] = {1,2,3,4,5,6,7,8,9,10};
    carray_repr_int(10, array, nullptr, 0);
    printf("\n");


    CArrayError error;
    int array2[10] = {1,2,3,4,5,6,7,8,9,10};
    error = carray_set_int(10, array2, 2, 88088);
    print("\nset(2, 88088)  error = %s\n", carray_err_str(error));
    set(10, array2, 3, -32565);
    printf("\n");
    carray_repr_int(10, array2, nullptr, 0);
    print("");

    double array3[10] = {1,2,3,4,5,6,7,8,9,10};
    error = carray_set_double(10, array3, 9, -4768.00036);
    print("\nset(9, -4768.00036)  error = %s\n", carray_err_str(error));
    carray_repr_double(10, array3, nullptr, 0);
    print("");

    double dbl = {};
    error = get(10, array3, 9, &dbl);
    show_error(error);
    print("get(9) = %f", dbl);

    error = get_or(10, array3, 67, &dbl, -666);
    show_error(error);
    print("get_or(67) = %f", dbl);

}



void t4(void) {
    printf("\nt4() test bool*:\n");
    bool * array[10] = {};
    bool b1 = true;
    bool b2 = false;
    carray_repr_bool_ptr(10, array, nullptr, 0);
    printf("\n");

    CArrayError error;
    error = carray_set_bool_ptr(10, array, 3, &b1);
    show_error(error);
    error = carray_set_bool_ptr(10, array, 6, &b2);
    show_error(error);
    carray_repr_bool_ptr(10, array, nullptr, 0);
    print("");

    repr(10, array, nullptr, 0);
    print("");
}

void t5(void) {
    printf("\nt5() test various repr:\n");
    short sarray1[10] = {1,2,3,4,5,6,7,8,9,10};
    unsigned char* uchar1[10] = {};
    unsigned char const* ucchar1[10] = {};
    void const* vcptr[10] = {};

    repr(10, sarray1, nullptr, 0);
    print("");

    repr(10, uchar1, nullptr, 0);
    print("");

    repr(10, ucchar1, nullptr, 0);
    print("");

    repr(10, vcptr, nullptr, 0);
    print("");


}

// make;
// clang -std=c23 -Wall -Werror -o ./out/carray_test.out carray_test.c carray_types.c --save-temps

// clang -std=c23 -Wall -Werror -o ../out/carray_test.out carray_test.c ../carray_types.c ../../dev_utils.c
int main(int argc, char *argv[]) {
    printf("STARTING carray_test.c");
    t1();
    t2();
    t3();
    print("");
    t4();
    t5();
}
