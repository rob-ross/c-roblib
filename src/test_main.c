//
// Created by Rob Ross on 2/1/26.
//

#include <stddef.h>
#include <stdio.h>

#include "psytest.h"
#include "punit.h"
#include "string_utils.h"

// make clang -std=c17 test_main.c string_utils.c punit.c psytest.c -o test_main.out

void test_PUNIT_TYPE(void) {
    // todo - unit test every supported type and verify the repr string is correct
    const int foo = 42;
    const int* foo_ptr = &foo;
    char *expected = "const int*";
    char *actual = PUNIT_TYPE(foo_ptr);
    if (sutil_strings_equal(expected, actual)) {
        printf("foo_ptr reported successfully as %s\n", actual);
    } else {
        printf("FAILED. foo_ptr reported as as %s, expected %s\n", actual, expected);
    }

}
void test_format_selection(void) {
    int foo = 42;
    printf( PUNIT_FMT(foo),  foo);
    printf("\n");

    const int foo_c = 43;
    printf( PUNIT_FMT(foo_c),  foo_c);
    printf("\n");

    int *foo_ptr = &foo;
    printf( PUNIT_FMT(foo_ptr),  foo_ptr);
    printf("\n");


}

// make: clang -std=c17 test_main.c punit.c -o test_main.out
int main(void) {
    test_format_selection();
    printf("\n");
    test_PUNIT_TYPE();
    // punit_assertFalse(1== 1);
    // punit_assertFalse(1== 1, "This is true.");
    //
    // punit_assertTrue(1 == 0);
    // punit_assertTrue(1 == 0, "Bad things happened.");
}
