//
// Created by Rob Ross on 2/20/26.
//

#include "../string_utils.h"

#include "../unit.h"

void test_sutil_char_at(void) {
    char const *fixture = "test";
    auto actual = sutil_char_at(fixture, 2);

    uint32_t expected = 's';

    assertEqual(expected, actual);
    assertEqual(expected, actual,  "index 2 should be 's'");
}

void test_sutil_char_at_out_of_range(void) {
    char const *fixture = "test";
    uint32_t actual = 0;

    // actual = sutil_char_at(fixture, 4);

    assertThrows(actual = sutil_char_at(fixture, 4));

    assertThrows(actual = sutil_char_at(nullptr, 4));

    actual = sutil_char_at(nullptr, 4);
    //
    // uint32_t expected = 0;
    // assertEqual(expected, actual);
    // assertEqual(expected, actual, "index 4 should be 0 (null terminator) or error");
}


// Is there a way to dynamically run tests for say, a given directory for any files starting with "test" and
// functions starting with "test_"?  There are no annotations in C but there are attributes. Could we use them
// for the same purpose?
// And we need to follow Java's model, since it is able to compile the source files it needs to run the tests against,
// including the tests.


// make:
// clang -std=c23 -DPUNIT_TEST_ASSERTS -I..  -o test_string_utils.out test_string_utils.c  ../string_utils.c ../unit.c
int main(int argc, char *argv[]) {
    test_case tests[] = { test_sutil_char_at, test_sutil_char_at_out_of_range, NULL};
    test_runner(tests);
}
