//
// Created by Rob Ross on 2/6/26.
//

#include <stdlib.h>

#include "string_utils.h"
// make:   clang -std=c17 -o test_string_utils.out test_string_utils.c string_utils.c




void test_sutil_ends_with(void) {
    printf("\ntest_sutil_ends_with\n");
    const char *str = "this is fun!";
    bool actual;

    actual = sutil_ends_with(str, "fun!");  // should return true
    printf("sutil_ends_with(%s, \"fun\") = %i, expected: 1\n", str, actual);

    actual = sutil_ends_with(str, "nope");  // should return false
    printf("sutil_ends_with(%s, \"nope\") = %i, expected: 0\n", str, actual);
}

void test_sutil_index() {
    printf("\ntest_sutil_index\n");
    const char *str1 = "This is a test string";
    const char *subs1 = "test";
    int expected = 10;
    int actual = sutil_index(str1, subs1);
    printf("expected: %i, actual: %i, expected == actual: %i\n", expected, actual, expected == actual);

    // actual = sutil_index_(str1, subs1);
    // printf("expected: %i, actual: %i, expected == actual: %i\n", expected, actual, expected == actual);

    expected = -1;
    actual = sutil_index(str1, "foo");
    printf("expected: %i, actual: %i, expected == actual: %i\n", expected, actual, expected == actual);

    expected = 15;
    actual = sutil_index(str1, "string");
    printf("expected: %i, actual: %i, expected == actual: %i\n", expected, actual, expected == actual);


    char *str2 = "This is a longer string. This has three 'this' in it. This is the third one.";
    expected = 41; // second "this"
    actual = sutil_index(str2, "this", 20);
    printf("expected: %i, actual: %i, expected == actual: %i\n", expected, actual, expected == actual);
    // actual = sutil_index_start(str2, "this", 20);
    // printf("expected: %i, actual: %i, expected == actual: %i\n", expected, actual, expected == actual);

    expected = 41;
    actual = sutil_index(str2, "this", 27, 45);
    printf("expected: %i, actual: %i, expected == actual: %i\n", expected, actual, expected == actual);
    // actual = sutil_index_start_end(str2, "this", 27, 45);
    // printf("expected: %i, actual: %i, expected == actual: %i\n", expected, actual, expected == actual);
}

void test_sutil_lower(void) {
    printf("\ntest_sutil_lower\n");
    const char *original = "THIS IS A LOWERCASE STRING. WITH *&^( SOME %OTHER CHAR4CER$";
    const char *expected = "this is a lowercase string. with *&^( some %other char4cer$";
    char *actual = sutil_lower(original);
    printf("expected: %s, actual: %s, expected == actual: %i\n", expected, actual, sutil_strings_equal(expected, actual));
    free(actual);
}

void test_sutil_pad_center(void) {
    printf("\ntest_sutil_pad_center\n");
    char *str1 = sutil_pad_center(" foo ", 10, '-');
    printf("'foo' centered 10 is : '%s'\n", str1);
    free(str1);
}

void test_sutil_pad_left(void) {
    printf("\ntest_sutil_pad_left\n");
    const char *original = "foo";
    const int width = 10;
    const char fill_char = '*';
    const char *expected = "*******foo";

    char *actual = sutil_pad_left(original,width, fill_char);
    printf("expected: %s, actual: %s, expected == actual: %i\n", expected, actual, sutil_strings_equal(expected, actual));
    free(actual);
}

void test_sutil_pad_right(void) {
    printf("\ntest_sutil_pad_right\n");
    const char *original = "foo";
    const int width = 10;
    const char fill_char = '*';
    const char *expected = "foo*******";

    char *actual = sutil_pad_right(original,width, fill_char);
    printf("expected: %s, actual: %s, expected == actual: %i\n", expected, actual, sutil_strings_equal(expected, actual));
    free(actual);
}

void test_sutil_starts_with(void) {
    printf("\ntest_sutil_starts_with\n");
    const char *str = "this is fun!";
    bool actual;

    actual = sutil_starts_with(str, "this");  // should return true
    printf("sutil_starts_with(%s, \"this\") = %i, expected: 1\n", str, actual);

    actual = sutil_starts_with(str, "nope");  // should return false
    printf("sutil_starts_with(%s, \"nope\") = %i, expected: 0\n", str, actual);
}

void test_sutil_strip(void) {
    printf("\ntest_sutil_strip:\n");

    const char *original = "     five leading and trailing spaces     ";
    const char *expected = "five leading and trailing spaces";
    char *actual = sutil_strip(original, NULL);
    printf("expected: %s, actual: %s, expected == actual: %i\n", expected, actual, sutil_strings_equal(expected, actual));
    free(actual);

    actual = sutil_strip(original, "");
    printf("expected: %s, actual: %s, expected == actual: %i\n", expected, actual, sutil_strings_equal(expected, actual));
    free(actual);

    actual = sutil_strip(original, " ");
    printf("expected: %s, actual: %s, expected == actual: %i\n", expected, actual, sutil_strings_equal(expected, actual));
    free(actual);

    actual = sutil_strip(original, "*");
    printf("expected: %s, actual: %s, expected == actual: %i\n", original, actual, sutil_strings_equal(original, actual));
    free(actual);
}

void test_sutil_strip_left(void) {
    printf("\ntest_sutil_strip_left:\n");

    const char *original = "     five leading spaces";
    const char *expected = "five leading spaces";
    char *actual = sutil_strip_left(original, NULL);
    printf("expected: %s, actual: %s, expected == actual: %i\n", expected, actual, sutil_strings_equal(expected, actual));
    free(actual);

    actual = sutil_strip_left(original, "");
    printf("expected: %s, actual: %s, expected == actual: %i\n", expected, actual, sutil_strings_equal(expected, actual));
    free(actual);

    actual = sutil_strip_left(original, " ");
    printf("expected: %s, actual: %s, expected == actual: %i\n", expected, actual, sutil_strings_equal(expected, actual));
    free(actual);

    actual = sutil_strip_left(original, "*");
    printf("expected: %s, actual: %s, expected == actual: %i\n", original, actual, sutil_strings_equal(original, actual));
    free(actual);
}


void test_sutil_strip_right(void) {
    printf("\ntest_sutil_strip_right:\n");

    const char *original = "five trailing spaces     ";
    const char *expected = "five trailing spaces";
    char *actual = sutil_strip_right(original, NULL);
    printf("expected: %s, actual: %s, expected == actual: %i\n", expected, actual, sutil_strings_equal(expected, actual));
    free(actual);

    actual = sutil_strip_right(original, "");
    printf("expected: %s, actual: %s, expected == actual: %i\n", expected, actual, sutil_strings_equal(expected, actual));
    free(actual);

    actual = sutil_strip_right(original, " ");
    printf("expected: %s, actual: %s, expected == actual: %i\n", expected, actual, sutil_strings_equal(expected, actual));
    free(actual);

    actual = sutil_strip_right(original, "*");
    printf("expected: %s, actual: %s, expected == actual: %i\n", original, actual, sutil_strings_equal(original, actual));
    free(actual);
}

void test_sutil_upper(void) {
    printf("\ntest_sutil_upper\n");
    const char *original = "this is a lowercase string. with *&^( some %other char4cer$";
    const char *expected = "THIS IS A LOWERCASE STRING. WITH *&^( SOME %OTHER CHAR4CER$";
    char *actual = sutil_upper(original);
    printf("expected: %s, actual: %s, expected == actual: %i\n", expected, actual, sutil_strings_equal(expected, actual));
    free(actual);
}

void display_defines(void) {
#ifdef __GNUC__
    printf("__GNUC__ is defined");
#else
    printf("__GNUC__ is not defined");
#endif
#ifdef __clang__
    printf("__clang__ is defined");
    #if __has_extension(statement_expressions)
        printf("   __has_extension(statement_expressions) is defined");
    #else
        printf("   __has_extension(statement_expressions) NOT is defined");

    #endif
#else
    printf("__clang__ is not defined");
#endif
}

// make:   clang -std=c17 -o test_string_utils.out test_string_utils.c string_utils.c

int main(int argc, char *argv[]) {
    test_sutil_ends_with();
    test_sutil_index();
    test_sutil_lower();
    test_sutil_pad_center();
    test_sutil_pad_left();
    test_sutil_pad_right();
    test_sutil_starts_with();
    test_sutil_strip();
    test_sutil_strip_left();
    test_sutil_strip_right();
    test_sutil_upper();

}
