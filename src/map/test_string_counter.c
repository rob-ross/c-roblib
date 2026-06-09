// test_string_counter.c
//
// Copyright (c) Rob Ross 2026. 
//
//
// Created 2026/03/03 22:32:05 PST

// ReSharper disable CppJoinDeclarationAndAssignment


#include <locale.h>
#include <stdio.h>
#include <time.h>

#include "string_counter.h"
#include "../testing_utils.h"
#include "hashmap.h"
#include "hashmap_private.h"

struct StringCounter {
    HashMap *map;
};

// -----------------------------------------------
// setup and teardown fixtures
// ----------------------------------------------
// create a StringCounter for use in test cases
static void *  sct_fixture(const MunitParameter params[], void* user_data) {
    StringCounter *sct = sct_create(16);
    munit_assert_not_null(sct);
    HashMap *map = sct->map;
    munit_assert_not_null(map);
    return sct;
}

// to free the StringCounter created by the sct_fixture after a test
static void sct_free(void * fixture) {
    sct_destroy((StringCounter*)fixture);
}




// -------------------------------------------------
// test cases
// -------------------------------------------------

static MunitResult test_create_0(const MunitParameter params[], void* fixture) {
    // test all default arguments
    StringCounter *sct;
    sct = sct_create();
    munit_assert_ptr_not_null(sct);
    HashMap *map = sct->map;
    munit_assert_ptr_not_null(map);
    munit_assert_int(16, ==, map->num_buckets);
    munit_assert_true(map_equals_MapDataPolicies(SCT_DEFAULT_DATA_POLICIES, map->data_policies));
    munit_assert_true(mem_equals_MemPolicy(MEM_DEFAULT_MALLOC_POLICY, map->mem_policy));
    sct_destroy(sct);
    return MUNIT_OK;
}

static MunitResult test_create_1(const MunitParameter params[], void* fixture) {
    // test passing num_buckets argument
    StringCounter *sct;
    sct = sct_create(0);
    munit_assert_int(16, ==, sct->map->num_buckets);
    munit_assert_true(map_equals_MapDataPolicies(SCT_DEFAULT_DATA_POLICIES, sct->map->data_policies));
    munit_assert_true(mem_equals_MemPolicy(MEM_DEFAULT_MALLOC_POLICY, sct->map->mem_policy));

    sct_destroy(sct);

    sct = sct_create(10);  //closest power of 2 is 16
    munit_assert_int(16, ==, sct->map->num_buckets);
    munit_assert_true(map_equals_MapDataPolicies(SCT_DEFAULT_DATA_POLICIES, sct->map->data_policies));
    munit_assert_true(mem_equals_MemPolicy(MEM_DEFAULT_MALLOC_POLICY, sct->map->mem_policy));
    sct_destroy(sct);

    sct = sct_create(20);  //closest power of 2 is 32
    munit_assert_int(32, ==, sct->map->num_buckets);
    munit_assert_true(map_equals_MapDataPolicies(SCT_DEFAULT_DATA_POLICIES, sct->map->data_policies));
    munit_assert_true(mem_equals_MemPolicy(MEM_DEFAULT_MALLOC_POLICY, sct->map->mem_policy));
    sct_destroy(sct);

    return MUNIT_OK;
}

static MapKey dummy_on_add_key(HashMap *map, MapKey key) {
    return (MapKey){};
}

static void dummy_on_free_key(HashMap *map, MapKey key) {}

static void dummy_on_free_context(void* context) {}


static ColValue dummy_on_set_value(HashMap *map, ColValue value) {
    return (ColValue){};
}

static void dummy_on_free_value(HashMap *map, ColValue value){}

int dummy_int = 42;
void * dummy_context = &dummy_int;

static MapDataPolicies data_policy_fixture() {
    return (MapDataPolicies){
        .key_policy={
            .context = dummy_context, .on_add_key   = dummy_on_add_key, .on_free_key = dummy_on_free_key,
                .on_free_context = dummy_on_free_context, .policy_type = COL_VALUE_POLICY_SHARED},
        .value_policy ={
            .context = dummy_context, .on_set_value = dummy_on_set_value,  .on_free_value = dummy_on_free_value,
            .on_free_context = dummy_on_free_context, .policy_type = COL_VALUE_POLICY_SHARED}
    };
}

void * dummy_alloc( void * context, size_t num_bytes ) { return calloc(1, num_bytes); }
void * dummy_calloc( void * context, size_t element_count, size_t element_size ) { return calloc(element_count, element_size); }
void * dummy_realloc( void * context, void * pointer, const size_t old_num_bytes, const size_t new_num_bytes ) {
    return realloc( pointer, new_num_bytes);
}
void dummy_free(  void * context, void * bytes){ free(bytes); }
void dummy_free_context(void * context ) {}


static MemPolicy mem_policy_fixture() {
    return (MemPolicy){
        .context = dummy_context, .alloc = dummy_alloc, .calloc = dummy_calloc, .realloc = dummy_realloc,
        .free = dummy_free, .free_context = dummy_free_context,
        .policy_type = MEM_POLICY_MALLOC_SHARED
    };
}

static MunitResult test_create_2(const MunitParameter params[], void* fixture) {
    // test passing num_buckets,  MapDataPolicies argument, default MemPolicy
    const MapDataPolicies data_policy = data_policy_fixture();
    StringCounter *sct;
    sct = sct_create(0, data_policy);
    munit_assert_int(16, ==, sct->map->num_buckets);
    munit_assert_true(map_equals_MapDataPolicies(data_policy, sct->map->data_policies));
    munit_assert_true(mem_equals_MemPolicy(MEM_DEFAULT_MALLOC_POLICY, sct->map->mem_policy));
    sct_destroy(sct);

    return MUNIT_OK;
}

static MunitResult test_create_3(const MunitParameter params[], void* fixture) {
    // test passing num_buckets, MapDataPolicies, and MemPolicy arguments
    const MapDataPolicies data_policy = data_policy_fixture();
    const MemPolicy       mem_policy = mem_policy_fixture();

    StringCounter *sct;
    sct = sct_create(0, data_policy, mem_policy);
    munit_assert_int(16, ==, sct->map->num_buckets);
    munit_assert_true(map_equals_MapDataPolicies(data_policy, sct->map->data_policies));
    munit_assert_true(mem_equals_MemPolicy(mem_policy, sct->map->mem_policy));
    sct_destroy(sct);

    return MUNIT_OK;
}

static MunitResult test_create_with_mempolicy(const MunitParameter params[], void* fixture) {
    // test passing num_buckets, MapDataPolicies, and MemPolicy arguments
    const MapDataPolicies data_policy = data_policy_fixture();
    const MemPolicy       mem_policy = mem_create_default_allocator(0);

    StringCounter *sct;
    sct = sct_create(0, data_policy, mem_policy);
    munit_assert_ptr_not_null(sct);
    HashMap *map = sct->map;
    munit_assert_ptr_not_null(map);

    munit_assert_int(16, ==, map->num_buckets);
    munit_assert_true(map_equals_MapDataPolicies(data_policy, sct->map->data_policies));
    munit_assert_true(mem_equals_MemPolicy(mem_policy, sct->map->mem_policy));

    sct_put(sct, "first", 1);
    sct_put(sct, "second", 1);
    sct_put(sct, "third", 1);

    sct_destroy(sct);

    return MUNIT_OK;
}
static MunitResult test_clear(const MunitParameter params[], void* fixture) {
    StringCounter *map = fixture;

    sct_put(map,"dog", 1);
    sct_put(map, "cat", 1);
    munit_assert_int(2, ==,  map->map->size);
    sct_clear( map );
    munit_assert_int(0, ==,  map->map->size);
    munit_assert_true(sct_is_empty(map));
    return MUNIT_OK;
}

static MunitResult test_contains_key(const MunitParameter params[], void* fixture) {
    StringCounter *map = fixture;

    sct_put(map,"dog",1);
    sct_put(map, "cat", 1);
    sct_put(map, "wolf",1 );

    munit_assert_true(sct_contains_key(map, "dog"));
    munit_assert_true(sct_contains_key(map, "cat"));
    munit_assert_true(sct_contains_key(map, "wolf"));
    munit_assert_false(sct_contains_key(map, "no such key"));

    return MUNIT_OK;
}

static MunitResult test_get(const MunitParameter params[], void* fixture) {
    StringCounter *map = fixture;
    ColValue value;

    sct_put(map,"dog",1);
    value = sct_get(map, key_for_string("dog"));
    munit_assert_int(1, ==, value.vlong);

    sct_put(map,"dog",11);
    value = sct_get(map, key_for_string("dog"));
    munit_assert_int(11, ==, value.vlong);

    return MUNIT_OK;
}

static MunitResult test_get_count(const MunitParameter params[], void* fixture) {
    StringCounter *map = fixture;
    long count;

    sct_put(map,"dog",1);
    count = sct_get_count(map, "dog");
    munit_assert_int(1, ==, count);

    sct_put(map,"dog",11);
    count = sct_get_count(map, "dog");
    munit_assert_int(11, ==, count);

    return MUNIT_OK;
}

static MunitResult test_is_empty(const MunitParameter params[], void* fixture) {
    StringCounter *map = fixture;
    sct_put(map,"dog", 1);
    sct_put(map, "cat", 1);
    munit_assert_false(sct_is_empty(map));

    sct_clear(map);
    munit_assert_true(sct_is_empty(map));

    return MUNIT_OK;
}

static MunitResult test_put(const MunitParameter params[], void* fixture) {
    StringCounter *map = fixture;
    sct_put(map,"dog", 1);
    sct_put(map, "cat", 22);

    munit_assert_int(1, ==, sct_get_count(map, "dog"));
    munit_assert_int(22, ==, sct_get_count(map, "cat"));
    munit_assert_int(2, ==, sct_size(map));

    return MUNIT_OK;

}

static MunitResult test_ref(const MunitParameter params[], void* fixture) {
    StringCounter *map = fixture;

    munit_assert_int(0, ==, sct_get_count(map, "dog"));

    char const *dog_literal = "dog";
    char const *dog_ref = sct_ref(map, "dog");

    munit_assert_not_null(dog_ref);
    munit_assert_ptr_not_equal(dog_literal, dog_ref);
    munit_assert_string_equal(dog_literal, dog_ref);
    munit_assert_int(1, ==, sct_get_count(map, "dog"));

    sct_ref(map, "dog");
    sct_ref(map, "dog");
    munit_assert_int(3, ==, sct_get_count(map, "dog"));

    return MUNIT_OK;

}

static MunitResult test_unref(const MunitParameter params[], void* fixture) {
    StringCounter *map = fixture;

    sct_put(map, "dog", 3);
    munit_assert_int(3, ==, sct_get_count(map, "dog"));
    sct_unref(map, "dog");
    sct_unref(map, "dog");

    munit_assert_int(1, ==, sct_get_count(map, "dog"));
    sct_unref(map, "dog");
    munit_assert_int(0, ==, sct_get_count(map, "dog"));
    munit_assert_false(sct_contains_key(map, "dog"));

    return MUNIT_OK;
}

static MunitResult test_remove(const MunitParameter params[], void* fixture) {
    StringCounter *map = fixture;
    sct_put(map,"dog", 1);
    sct_put(map, "cat", 22);

    munit_assert_int(1, ==, sct_get_count(map, "dog"));
    munit_assert_int(22, ==, sct_get_count(map, "cat"));
    munit_assert_int(2, ==, sct_size(map));

    sct_remove(map, "dog");
    munit_assert_int(0, ==, sct_get_count(map, "dog"));
    munit_assert_false(sct_contains_key(map, "dog"));

    munit_assert_int(22, ==, sct_get_count(map, "cat"));
    munit_assert_int(1, ==, sct_size(map));

    return MUNIT_OK;
}

static MunitResult test_size(const MunitParameter params[], void* fixture) {
    StringCounter *map = fixture;
    sct_put(map,"dog", 1);
    sct_put(map, "cat", 22);

    munit_assert_int(1, ==, sct_get_count(map, "dog"));
    munit_assert_int(22, ==, sct_get_count(map, "cat"));
    munit_assert_int(2, ==, sct_size(map));

    sct_remove(map, "dog");
    munit_assert_int(1, ==, sct_size(map));

    sct_remove(map, "cat");
    munit_assert_int(0, ==, sct_size(map));

    return MUNIT_OK;
}

static void create_1M_unique_strings(StringCounter *sct) {
    //put 1M unique strigs into the StringCounter.
    int n = 1'000'000; // 1M
    for (int i = 0; i < n; ++i) {
        char strkey[15]; // max 7 chars for value of i, plus 5 for 'hello', plus terminator = 13
        snprintf(strkey, 15, "hello%d",i);
        sct_put( sct, strkey, i );
        long vlong = sct_get_count(sct, strkey );
        munit_assert_int( i, ==, vlong);
    }

}

static double elapsed_seconds(struct timespec start, struct timespec end) {
    return (double)(end.tv_sec - start.tv_sec)
         + (double)(end.tv_nsec - start.tv_nsec) / 1e9;
}

[[maybe_unused]]
static void print_elapsed(const char *label, const struct timespec start, const struct timespec end) {
    printf("%s: %.6f seconds\n", label, elapsed_seconds(start, end));
}

// this should be part of a performance test, i.e., complete method in under X time.
[[maybe_unused]]
static MunitResult time_put_1M_unique_strings(const MunitParameter params[], void* fixture) {
    struct timespec start;
    struct timespec end;

    StringCounter *sct_malloc = sct_create();
    double put_time;
    double destroy_time;
    //start timer
    clock_gettime(CLOCK_MONOTONIC, &start);
    create_1M_unique_strings(sct_malloc);
    //stop timer and display time
    clock_gettime(CLOCK_MONOTONIC, &end);
    put_time = elapsed_seconds(start, end);

    clock_gettime(CLOCK_MONOTONIC, &start);
    sct_destroy(sct_malloc);
    clock_gettime(CLOCK_MONOTONIC, &end);
    destroy_time = elapsed_seconds(start, end);
    // print_elapsed("malloc policy", start, end);
    printf("malloc policy: put: %.6g s, destroy: %.6g s\n", put_time, destroy_time);


    // now create a StringCounter that uses a memory pool
    const MapDataPolicies data_policy = SCT_DEFAULT_DATA_POLICIES;
    const MemPolicy       mem_policy = mem_create_default_allocator(0);
    StringCounter *sct_allocator = sct_create(0, data_policy, mem_policy);

    //start timer
    clock_gettime(CLOCK_MONOTONIC, &start);
    create_1M_unique_strings(sct_allocator);
    //stop timer and display time
    clock_gettime(CLOCK_MONOTONIC, &end);
    put_time = elapsed_seconds(start, end);

    clock_gettime(CLOCK_MONOTONIC, &start);
    sct_destroy(sct_allocator);
    clock_gettime(CLOCK_MONOTONIC, &end);
    destroy_time = elapsed_seconds(start, end);
    // print_elapsed("allocator policy", start, end);

    printf("allocator policy: put: %.6g s, destroy: %.6g s\n", put_time, destroy_time);


    return MUNIT_OK;
}

// todo test StringCounter as string pool for a HashMap.


static void create_1M_identical_strings(HashMap *map) {

    constexpr size_t N = 10'000'000;
    constexpr char value_str[] = "hello world";
    // ColValue initial_value;

    // map_put(map, 0, value_str );
    // initial_value = map_get(map, 0);  //this value should be the same every time we put this string
    // printf("initial_value.vstring = '%s', vstring ptr: %p\n", initial_value.vstring, initial_value.vstring);
    //
    // map_put(map, 1, value_str );
    // initial_value = map_get(map, 1);  //this value should be the same every time we put this string
    // printf("initial_value.vstring = '%s', vstring ptr: %p\n", initial_value.vstring, initial_value.vstring);

    for (int i = 0; i < N; ++i) {
        map_put(map, i, value_str );
        ColValue cv = map_get(map, i);
        // printf("cv.vstring = '%s', vstring ptr: %p\n", cv.vstring, cv.vstring);
        munit_assert_string_equal( value_str, cv.vstring);
        // munit_assert_ptr_equal(initial_value.vstring, cv.vstring);
    }
}

[[maybe_unused]]
static MunitResult time_put_1M_identical_strings(const MunitParameter params[], void* fixture) {
    struct timespec start;
    struct timespec end;

    double put_time;
    double destroy_time;
    HashMap *map = map_create();
    //start timer
    clock_gettime(CLOCK_MONOTONIC, &start);
    create_1M_identical_strings(map);
    //stop timer and display time
    clock_gettime(CLOCK_MONOTONIC, &end);
    put_time = elapsed_seconds(start, end);

    clock_gettime(CLOCK_MONOTONIC, &start);
    map_destroy(map);
    clock_gettime(CLOCK_MONOTONIC, &end);
    destroy_time = elapsed_seconds(start, end);
    printf("\nmalloc policy: put: %.6g s, destroy: %.6g s\n", put_time, destroy_time);


    // now create a HashMap backed by a StringCounter string pool
    // const MapDataPolicies data_policy = SCT_DEFAULT_DATA_POLICIES;
    const MemPolicy       mem_policy = mem_create_default_allocator(0);
    // StringCounter *sct_allocator = sct_create(0, data_policy, mem_policy);
    HashMap *sp_map = map_create_using_stringpool(10'000'000, &mem_policy);
    //start timer
    clock_gettime(CLOCK_MONOTONIC, &start);
    create_1M_identical_strings(sp_map);
    //stop timer and display time
    clock_gettime(CLOCK_MONOTONIC, &end);
    put_time = elapsed_seconds(start, end);

    clock_gettime(CLOCK_MONOTONIC, &start);
    map_destroy(sp_map);
    clock_gettime(CLOCK_MONOTONIC, &end);
    destroy_time = elapsed_seconds(start, end);

    printf("\nStrincCounter string pool: put: %.6g s, destroy: %.6g s\n", put_time, destroy_time);

    return MUNIT_OK;
}

// ------------------------------------
// end test cases
// ------------------------------------


// make
// clang -std=c23 -fsanitize=address -fsanitize=leak -Wall -Werror -o ./out/test_string_counter.out test_string_counter.c string_counter.c hashmap.c ../memory/memory_pool.c ../munit/munit.c
//
//  clang -std=c23 -Wall -Werror -o ./out/test_string_counter.out test_string_counter.c string_counter.c hashmap.c ../memory/memory_pool.c ../munit/munit.c
//
int main_test_string_counter(int argc, char *argv[argc + 1]) {
    setlocale(LC_NUMERIC, "en_US.UTF-8");   // use user's system locale

    MunitTest tests[] = {
        munit_test(test_create_0),
        munit_test(test_create_1),
        munit_test(test_create_2),
        munit_test(test_create_3),
        munit_test(test_create_with_mempolicy),
        munit_test(test_clear),
        munit_test(test_contains_key),
        munit_test(test_get),
        munit_test(test_get_count),
        munit_test(test_is_empty),
        munit_test(test_put),
        munit_test(test_ref),
        munit_test(test_unref),
        munit_test(test_remove),
        munit_test(test_size),

        MUNIT_NULL_TEST,
    };

    apply_fixture(tests, sct_fixture, sct_free);

[[maybe_unused]]
     MunitSuite suite = {
        "/test_string_counter", /* name */
        tests, /* tests */
        nullptr, /* suites */
        1, /* iterations */
        MUNIT_SUITE_OPTION_NONE /* options */
      };


    int result = 0;
    result = munit_suite_main(&suite, nullptr, argc, argv);


    // time_put_1M_unique_strings(nullptr, nullptr);
    // time_put_1M_identical_strings(nullptr, nullptr);

    return result;

}

#ifdef TEST_STRING_COUNTER
int main(int argc, char *argv[argc + 1]) {
    return main_test_string_counter(argc, argv);

}
#endif
