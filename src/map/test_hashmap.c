// test_hashmap.c
//
// Copyright (c) Rob Ross 2026.
//
//

#include <locale.h>
#include <stdio.h>

#include "../testing_utils.h"
#include "hashmap.h"
#include <stdlib.h>
#include <string.h>

#include "string_counter.h"


// -----------------------------------------------
// setup and teardown fixtures
// ----------------------------------------------
// create a HashMap for use in test cases
static void *  hashmap_fixture(const MunitParameter params[], void* user_data) {
    HashMap *map = map_create();
    munit_assert_not_null(map);
    return map;
}

// to free the hashmap created by the hashmap_fixture after a test
static void hashmap_free(void * fixture) {
    map_destroy(fixture);
}

// -------------------------------------------------
// test cases
// -------------------------------------------------

static MunitResult test_create_3(const MunitParameter params[], void* fixture) {
    HashMap *map = map_create(10);
    munit_assert_ptr_not_null(map);
    map_destroy(map);
    return MUNIT_OK;
}

static MunitResult test_put_and_get_int(const MunitParameter params[], void* fixture) {
    HashMap *map = fixture;
    // printf("test_put_and_get_int: \n");
    map_put(map, 1, 42);
    ColValue retrieved = map_get(map, 1);
    munit_assert_int(retrieved.vlong, ==, 42);

    int foo = 2;
    int bar = 67;
    map_put(map, foo, bar);
    retrieved = map_get(map, foo);
    munit_assert_int(retrieved.vlong, ==, bar);
    // printf("returning from test_put_and_get_int: \n");

    return MUNIT_OK;
}

static MunitResult test_put_and_get_string(const MunitParameter params[], void* fixture) {
    HashMap *map = fixture;
    map_put(map, 1, "hello");
    ColValue retrieved = map_get(map, 1);
    munit_assert_string_equal(retrieved.vstring, "hello");
    return MUNIT_OK;
}

static MunitResult test_refcount(const MunitParameter params[], void* fixture) {
    HashMap *map = fixture;
    map_put(map, 1, "hello");
    map_put(map, 1, "world"); // This should free val1 and replace it with val2
    ColValue retrieved = map_get(map, 1);
    munit_assert_string_equal(retrieved.vstring, "world");
    return MUNIT_OK;
}

static MunitResult test_remove_key(const MunitParameter params[], void* fixture) {
    HashMap *map = fixture;
    map_put(map, 1, "hello");
    map_remove(map, 1);
    ColValue retrieved = map_get(map, 1);
    munit_assert_int(retrieved.value_type, ==, COL_TYPE_NULL);
    munit_assert_ptr_null(retrieved.vvoid_ptr);
    return MUNIT_OK;
}

static MunitResult test_put_and_get_str_key(const MunitParameter params[], void* fixture) {
    HashMap *map = fixture;

    map_put(map, "hello", "world");
    map_put(map, "good", "morning");
    ColValue got_char = map_get(map, "hello");
    munit_assert_string_equal("world", got_char.vstring);

    map_remove(map, "good");
    ColValue retrieved = map_get(map, "good");
    munit_assert_ptr_null(retrieved.vstring);

    return MUNIT_OK;
}

static MunitResult test_put_and_get_bool_values(const MunitParameter params[], void* fixture) {
    HashMap *map = fixture;
    bool b1 = false;
    map_put(map, "false", false);
    map_put(map, "true", !b1);

    ColValue result1 = map_get(map, "false");
    ColValue result2 = map_get(map, "true");

    munit_assert_true(result2.vlong);
    munit_assert_false(result1.vlong);

    return MUNIT_OK;
}

static MunitResult test_klong_vstring(const MunitParameter params[], void* fixture) {
    HashMap *map = fixture;
    for (int i = 0; i < 100; ++i) {
        char search_string[10]; // max 4 chars for value of i, plus 5 for 'hello', plus terminator
        snprintf(search_string, 10, "hello%d",i+1);
        map_put(map, i, search_string );
    }
    for (int i=0; i< 100; ++i) {
        char search_string[10]; // max 4 chars for value of i, plus 5 for 'hello', plus terminator
        snprintf(search_string,10, "hello%d",i+1);

        ColValue value = map_get(map, i );

        munit_assert_string_equal( search_string, value.vstring);
    }

    return MUNIT_OK;
}

static MunitResult test_generic_put(const MunitParameter params[], void* fixture) {
    HashMap *map = fixture;
    map_put(map, 42, "foo");
    map_put(map, "bar", 123);
    map_put(map, (short)67, "short!");
    map_put(map, (float)67.767, "float!");

    ColValue retrieved_str = map_get(map, 42);
    munit_assert_string_equal(retrieved_str.vstring, "foo");

    ColValue retrieved_long = map_get(map, "bar");
    munit_assert_long(retrieved_long.vlong, ==, 123);

    return MUNIT_OK;
}

static MunitResult test_clear(const MunitParameter params[], void* fixture) {
    HashMap *map = fixture;

    map_put(map, 1, "dog");
    map_put(map, 2, "cat");
    munit_assert_int(2, ==,  map->size);
    map_clear( map );
    munit_assert_int(0, ==,  map->size);
    munit_assert_true(map_is_empty(map));
    return MUNIT_OK;
}

static MunitResult test_contains_key(const MunitParameter params[], void* fixture) {
    HashMap *map = fixture;

    map_put(map, 1, "dog");
    map_put(map, 2.0, "cat");
    map_put(map, "wolf", "big bad");
    int int1 = 42;
    void * vptr = (void *)&int1;
    map_put(map, vptr, "void pointer");

    munit_assert_true(map_contains_key(map, 1));
    munit_assert_true(map_contains_key(map, 2.0));
    munit_assert_true(map_contains_key(map, "wolf"));
    munit_assert_true(map_contains_key(map, vptr));

    return MUNIT_OK;

}

static MunitResult test_contains_value(const MunitParameter params[], void* fixture) {
    HashMap *map = fixture;

    map_put(map, 1, 1);
    map_put(map, 2.0, 2.0);
    map_put(map, "wolf", "big bad");
    int int1 = 42;
    void * vptr = (void *)&int1;
    map_put(map, vptr, vptr );

    munit_assert_true(map_contains_value(map, 1));
    munit_assert_true(map_contains_value(map, 2.0));
    munit_assert_true(map_contains_value(map, "big bad"));
    munit_assert_true(map_contains_value(map, vptr));

    return MUNIT_OK;
}

static MunitResult test_get_or(const MunitParameter params[], void* fixture) {
    HashMap *map = fixture;

    map_put(map, 1, 1);
    map_put(map, 2.0, 2.0);
    map_put(map, "three", "three");
    int int1 = 42;
    void * vptr = (void *)&int1;
    map_put(map, vptr, vptr );

    ColValue result = map_get_or(map, 76, 67);

    munit_assert_int(67, ==, result.vlong );
    result = map_get_or(map, 7.6, 6.7);
    munit_assert_double(6.7, ==, result.vdouble );
    result = map_get_or(map, "four", "forty two");
    munit_assert_string_equal("forty two", result.vstring);

    constexpr short short1 = (const short)99;
    void * sptr = (void*) &short1;

    void * missing_key = &"not here";

    result = map_get_or(map, missing_key, sptr);
    munit_assert_int(99, ==, *(const short*)result.vvoid_ptr );

    return MUNIT_OK;
}

static MunitResult test_try_get(const MunitParameter params[], void* fixture) {
    HashMap *map = fixture;

    map_put(map, 1, 1);
    map_put(map, 2.0, 2.0);
    map_put(map, "wolf", "big bad");
    int int1 = 42;
    void * vptr = (void *)&int1;
    map_put(map, vptr, vptr );

    ColValue mv = {};
    munit_assert_true(map_try_get(map, 1, &mv));
    munit_assert_int(1, ==, mv.vlong);
    munit_assert_true(map_try_get(map, 2.0, &mv));
    munit_assert_int(2.0, ==, mv.vdouble);
    munit_assert_true(map_try_get(map, "wolf", &mv));
    munit_assert_string_equal("big bad", mv.vstring);
    munit_assert_true(map_try_get(map, vptr, &mv));
    munit_assert_int(42, ==, *(int*)mv.vvoid_ptr);

    munit_assert_false(map_try_get(map, 22, &mv));
    munit_assert_int(COL_TYPE_NULL, ==, mv.value_type);

    return MUNIT_OK;
}

static MunitResult test_size(const MunitParameter params[], void* fixture) {
    HashMap *map = fixture;

    constexpr size_t n = 20;
    for (size_t index = 0; index < n; ++index ) {
        map_put(map, index, index);
        munit_assert_int(index + 1, ==, map_size(map));
    }

    return MUNIT_OK;
}

// ------------------------------------
// end test cases
// ------------------------------------

static MunitResult test_10K_inserts(const MunitParameter params[], void* fixture) ;
static MunitResult test_10K_string_inserts(const MunitParameter params[], void* fixture) ;


// make
//
//
//  clang -std=c23 -Wall -Werror -o ./out/test_hashmap.out test_hashmap.c string_counter.c hashmap.c ../memory/memory_pool.c ../munit/munit.c
int main_test_hashmap(int argc, char *argv[argc + 1]) {
    setlocale(LC_NUMERIC, "en_US.UTF-8");   // use user's system locale

    MunitTest tests[] = {
        munit_test(test_create_3),
        munit_test("/test_put_and_get_int", test_put_and_get_int),
        munit_test(test_put_and_get_string),
        munit_test(test_refcount),
        munit_test(test_remove_key),
        munit_test(test_put_and_get_str_key),
        munit_test(test_put_and_get_bool_values),
        munit_test(test_klong_vstring),
        munit_test(test_generic_put),
        munit_test(test_clear),
        munit_test(test_contains_key),
        munit_test(test_contains_value),
        munit_test(test_get_or),
        munit_test(test_try_get),
        munit_test(test_size),

        MUNIT_NULL_TEST,
    };

    apply_fixture(tests, hashmap_fixture, hashmap_free);

[[maybe_unused]]
     MunitSuite suite = {
        "/test_hashmap", /* name */
        tests, /* tests */
        nullptr, /* suites */
        1, /* iterations */
        MUNIT_SUITE_OPTION_NONE /* options */
      };


    int result = 0;
    result = munit_suite_main(&suite, nullptr, argc, argv);

    // munit spawns a process for each test and that confuses the debugger. To debug a failing test we must run it
    // manually in the main thread. examples:
    // test_put_and_get_string(nullptr, hashmap_fixture(nullptr, nullptr));
    // test_10K_inserts(nullptr, nullptr);
    // test_put_and_get_int(nullptr, hashmap_fixture(nullptr, nullptr));
    // test_create_3(nullptr, hashmap_fixture(nullptr, nullptr));
    // test_10K_string_inserts(nullptr, nullptr);

    // test_klong_vstring(nullptr, hashmap_fixture(nullptr, nullptr));

    // display_type_sizes();


    return result;

}

#ifdef TEST_HASHMAP
int main(int argc, char *argv[argc + 1]) {
    return main_test_hashmap(argc, argv);

}
#endif

// -------------------------
// ad hoc tests
// -------------------------
[[maybe_unused]]
static void test_repr(void) {
    HashMap *map = map_create(0);
    map_repr_HashMap(map, true, ""); print("");
    map_destroy(map);

    map_create(0);
    map_repr_HashMap(map, true, ""); print("");
    map_destroy(map);

    map = map_create(16);
    map_repr_HashMap(map, true, ""); print("");
    map_destroy(map);

    map = map_create(17);

    map_destroy(map);
}

[[maybe_unused]]
static MunitResult test_10K_inserts(const MunitParameter params[], void* fixture) {
    print("test_10K_inserts");
    // constexpr size_t N = 100'000'000;
    constexpr size_t N = 1'000'000;

    HashMap *map = map_create(0);
    size_t buffer_size = 20;  // max 9 chars for value of i, plus 5 for 'hello', plus terminator
    for (int i = 0; i < N; ++i) {
        char search_string[buffer_size] = {};
        snprintf(search_string, buffer_size, "hello%d",i+1);
        map_put(map, i, search_string );
    }
    for (int i=0; i< N; ++i) {
        char search_string[buffer_size] = {}; // max 4 chars for value of i, plus 5 for 'hello', plus terminator
        snprintf(search_string,buffer_size, "hello%d",i+1);
        [[maybe_unused]]ColValue value = map_get(map, i );
        // print("search string = %s, value = %s", search_string, value);

        munit_assert_string_equal( search_string, value.vstring);
    }

    print("");

    printf("finished adding %'zu items to map.\n", N);
    map_repr_HashMap(map, false, "");


    fflush(stdout);
    // printf("about to call map_destroy:\n");
    fflush(stdout);

    map_destroy(map);

    return MUNIT_OK;
}

[[maybe_unused]]
static MunitResult test_10K_string_inserts(const MunitParameter params[], void* fixture) {
    print("test_10K_string_inserts");
    constexpr size_t N = 10;
    HashMap *map = map_create_using_stringpool(16, nullptr);

    StringCounter *string_pool = map->data_policies.value_policy.context;

    print("map: %p, string_pool: %p", map, string_pool);


    char * value_str = "hello world";
    for (int i = 0; i < N; ++i) {
        map_put(map, i, value_str );
        print("map_put(map, %d, %s)", i, value_str);
    }
    print("done with map_put");

    for (int i=0; i < N; ++i) {
        ColValue value = map_get(map, i );
        printf("\nmap_get(map, %d) ", i);
        map_repr_MapValue(value, true);
        fflush(stdout);
        munit_assert_string_equal( value_str, value.vstring);
    }

    print("\ndone with map_get");

    printf("finished adding %zu items to map.\n", N);
    map_repr_HashMap(map, false, "");

    if (string_pool) {
        long count = sct_get_count(string_pool, "hello world");
        printf("'hello world' ref count = %ld\n", count);
        sct_repr_StringCounter(string_pool, false);
    }


    print("\n");

    //try removing keys, the reference count should reduce by 1 each time
    for (int i=0; i< N; ++i) {
        long count = 0;
        print("\nremoving #%d", i);
        map_remove(map, i );
        munit_assert_int( N-i - 1, ==, map->size);
        if (string_pool) {
            count = sct_get_count(string_pool, "hello world");
            munit_assert_int( N-i - 1, ==, count);
            printf("'hello world' ref count = %ld\n", count);
            sct_repr_StringCounter(string_pool, false);
        }

    }

    //fflush(stdout);
    printf("about to call map_destroy:\n");
   // fflush(stdout);
    map_destroy(map);

    return MUNIT_OK;
}