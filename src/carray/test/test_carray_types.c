//
//  test_carray_types.c
//
// Created by Rob Ross on 2/25/26.
//

#include "../../testing_utils.h"
#include "../carray_types.h"

// munit formats error messages as expected, actual

typedef struct UCharStr { unsigned char err; const char *expected;} UCharStr;



// ------------------------------------
// common functions
// ------------------------------------

//allocate a buffer for the data and copy source data into the new buffer. Return the new buffer.
//caller must free pointer
void * allocate_fixture(void * data, size_t data_size) {
    void *p = malloc(data_size);
    if (!p) {
        exit(EXIT_FAILURE);
    }
    memcpy(p, data, data_size);
    return p;
}

// just frees its argument
static void
free_tear_down(void* fixture) {
    free(fixture);
}


// ------------------------------------
// test_carray_err_str
// ------------------------------------

void * test_carray_err_str_setup(const MunitParameter params[], void* user_data) {
    UCharStr test_data[] = {
        {CARRAY_OK, "ok"},
        {CARRAY_ERR_NULL_ARG, "null argument"},
        {CARRAY_ERR_OUT_OF_MEM, "out of memory"},
        {CARRAY_ERR_EMPTY, "empty array"},
        {CARRAY_ERR_INDEX_OUT_OF_RANGE, "index out of range"},
        {CARRAY_ERR_UNNOWN_ERR, "unknown error"},
        {(unsigned char) (-1), "unknown error"},
        NULL_ENUM_STR
    };
    return allocate_fixture(test_data, sizeof(test_data));
}



MunitResult test_carray_err_str(const MunitParameter params[], void* fixture) {
    //user_data_or_fixture will be passed the result of the setup function if defined for this test.
    // we are used to using Lists of tuples for this kind of thing in Java.
    // for this specific test, for each case we have:
    // enum constant, used to pass to the function under test (carray_err_str)
    // expected value, in this case a char*
    // that's it. We call the FUT (Function under test) with the enum constant (fixture parameter), and compare
    // the actual result to the expected result using assert_string_equal()

    // we can use a struct to contain these two datums
    const UCharStr *ptr = fixture;
    while (ptr->expected != nullptr) {
        munit_assert_string_equal(ptr->expected, carray_err_str(ptr->err));
        ptr++;
    }
    return MUNIT_OK;
}

// kept here as an example of a simple test case method
MunitResult test_carray_err_str0(const MunitParameter params[], void* user_data_or_fixture) {
    munit_assert_string_equal("ok", carray_err_str(CARRAY_OK));
    munit_assert_string_equal("null argument", carray_err_str(CARRAY_ERR_NULL_ARG));

    munit_assert_string_equal("out of memory", carray_err_str(CARRAY_ERR_OUT_OF_MEM));
    munit_assert_string_equal("empty array", carray_err_str(CARRAY_ERR_EMPTY));
    munit_assert_string_equal("index out of range", carray_err_str(CARRAY_ERR_INDEX_OUT_OF_RANGE));
    munit_assert_string_equal("unknown error", carray_err_str((unsigned char)-1));

    // if a preceeding test fails, this line is not reached
    return MUNIT_OK;
}



// ------------------------------------
// test_ctype_repr_str
// ------------------------------------

void * test_ctype_repr_str_setup(const MunitParameter params[], void* user_data) {
    UCharStr test_data[] = {
        { CTYPE_BOOL,           "bool" },
        { CTYPE_BOOL_PTR,       "bool*" },
        { CTYPE_BOOL_CPTR,      "bool const*" },
        { CTYPE_CHAR,           "char" },
        { CTYPE_CHAR_PTR,       "char*" },
        { CTYPE_CHAR_CPTR,      "char const*" },
        { CTYPE_UCHAR,          "unsigned char" },
        { CTYPE_UCHAR_PTR,      "unsigned char*" },
        { CTYPE_UCHAR_CPTR,     "unsigned char const*" },
        { CTYPE_SCHAR,          "signed char" },
        { CTYPE_SCHAR_PTR,      "signed char*" },
        { CTYPE_SCHAR_CPTR,     "signed char const*" },
        { CTYPE_SHORT,          "short" },
        { CTYPE_SHORT_PTR,      "short*" },
        { CTYPE_SHORT_CPTR,     "short const*" },
        { CTYPE_USHORT,         "unsigned short" },
        { CTYPE_USHORT_PTR,     "unsigned short*" },
        { CTYPE_USHORT_CPTR,    "unsigned short const*" },
        { CTYPE_INT,            "int" },
        { CTYPE_INT_PTR,        "int*" },
        { CTYPE_INT_CPTR,       "int const*" },
        { CTYPE_UINT,           "unsigned int" },
        { CTYPE_UINT_PTR,       "unsigned int*" },
        { CTYPE_UINT_CPTR,      "unsigned int const*" },
        { CTYPE_LONG,           "long" },
        { CTYPE_LONG_PTR,       "long*" },
        { CTYPE_LONG_CPTR,      "long const*" },
        { CTYPE_ULONG,          "unsigned long" },
        { CTYPE_ULONG_PTR,      "unsigned long*" },
        { CTYPE_ULONG_CPTR,     "unsigned long const*" },
        { CTYPE_LLONG,          "long long" },
        { CTYPE_LLONG_PTR,      "long long*" },
        { CTYPE_LLONG_CPTR,     "long long const*" },
        { CTYPE_ULLONG,         "unsigned long long" },
        { CTYPE_ULLONG_PTR,     "unsigned long long*" },
        { CTYPE_ULLONG_CPTR,    "unsigned long long const*" },
        { CTYPE_FLOAT,          "float" },
        { CTYPE_FLOAT_PTR,      "float*" },
        { CTYPE_FLOAT_CPTR,     "float const*" },
        { CTYPE_DOUBLE,         "double" },
        { CTYPE_DOUBLE_PTR,     "double*" },
        { CTYPE_DOUBLE_CPTR,    "double const*" },
        { CTYPE_LDOUBLE,        "long double" },
        { CTYPE_LDOUBLE_PTR,    "long double*" },
        { CTYPE_LDOUBLE_CPTR,   "long double const*" },
        { CTYPE_FCOMPLEX,       "float _Complex" },
        { CTYPE_FCOMPLEX_PTR,   "float _Complex*" },
        { CTYPE_FCOMPLEX_CPTR,  "float _Complex const*" },
        { CTYPE_DCOMPLEX,       "double _Complex" },
        { CTYPE_DCOMPLEX_PTR,   "double _Complex*" },
        { CTYPE_DCOMPLEX_CPTR,  "double _Complex const*" },
        { CTYPE_LDCOMPLEX,      "long double _Complex" },
        { CTYPE_LDCOMPLEX_PTR,  "long double _Complex*" },
        { CTYPE_LDCOMPLEX_CPTR, "long double _Complex const*" },
        { CTYPE_VOID_PTR,       "void*" },
        { CTYPE_VOID_CPTR,      "void const*" },
        NULL_ENUM_STR
    };
    return allocate_fixture(test_data, sizeof(test_data));
}

MunitResult test_ctype_repr_str(const MunitParameter params[], void* fixture) {
    const UCharStr *ptr = fixture;
    while (ptr->expected != nullptr) {
        munit_assert_string_equal(ptr->expected, ctype_repr_str(ptr->err));
        ptr++;
    }
    return MUNIT_OK;
}

// ------------------------------------
// test suite setup
// ------------------------------------

MunitTest tests[] = {
    {
        .name="/test_carray_err_str", .test=test_carray_err_str, .setup=test_carray_err_str_setup,
        .tear_down=free_tear_down, .options=MUNIT_TEST_OPTION_NONE, .parameters=nullptr
      },
    {
        .name="/test_ctype_repr_str", .test=test_ctype_repr_str, .setup=test_ctype_repr_str_setup,
        .tear_down=free_tear_down, .options=MUNIT_TEST_OPTION_NONE, .parameters=nullptr
      },
      /* Mark the end of the array with an entry where the test
       * function is nullptr */
      { nullptr, nullptr, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr }
};

static const MunitSuite suite = {
    "/carray", /* name */
    tests, /* tests */
    nullptr, /* suites */
    1, /* iterations */
    MUNIT_SUITE_OPTION_NONE /* options */
  };


// make:

/*
clang -std=c23 -o test_carray_types.out -Wall -Wextra -Wconversion -Wsign-conversion -Wno-unused-parameter -Wno-unused-variable  -Werror \
test_carray_types.c ../carray_types.c -Wno-conversion -Wno-sign-conversion  ../../munit/munit.c

*/

int main (int argc, char* argv[argc + 1]) {
        return munit_suite_main(&suite, nullptr, argc, argv);
    }

/*
# Compile your code with strict warnings
clang -std=c23 -Wall -Wextra -Wconversion -Wsign-conversion -Werror -Wno-unused-parameter \
  -c test_carray_types.c -o test_carray_types.o

clang -std=c23 -Wall -Wextra -Wconversion -Wsign-conversion -Werror \
  -c ../carray_types.c -o carray_types.o

# Compile munit with relaxed warnings
clang -std=c23 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-conversion -Wno-sign-conversion \
  -c ../../munit/munit.c -o munit.o

# Link
clang test_carray_types.o carray_types.o munit.o -o test_carray_types.out

*/
