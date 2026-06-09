// test_hashmap.c
//
// Copyright (c) Rob Ross 2026.
//
//

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashmap.h"
#include "hashmap_private.h"
#include "../testing_utils.h"



// -----------------------------------------------
// setup and teardown fixtures
// ----------------------------------------------

typedef struct TestCase {
    MapKey    k;
    ColValue  v;
} TestCase;

typedef struct FixtureStruct {
    HashMap  *map;
    unsigned num_test_cases;
    TestCase test_data[];
} FixtureStruct;

// create a HashMap for use in test cases
static void *  hashmap_fixture(const MunitParameter params[], void* user_data) {
    HashMap *map = map_create();
    munit_assert_not_null(map);
    FixtureStruct *fs = malloc(sizeof(HashMap) + ( 9 * sizeof(TestCase)) );
    fs->map = map;
    TestCase *d = fs->test_data;
    d[0] = (TestCase){.k = key_for_string("one"), value_for_long(1)};
    d[1] = (TestCase){.k = key_for_string("two"), value_for_double(2.2)};
    d[2] = (TestCase){.k = key_for_string("three"), value_for_string("three")};

    d[3] = (TestCase){.k = key_for_long(1), value_for_long(1)};
    d[4] = (TestCase){.k = key_for_long(2), value_for_double(1.0)};
    d[5] = (TestCase){.k = key_for_long(3), value_for_string("three")};

    d[6] = (TestCase){.k = key_for_double(1.1), value_for_long(1)};
    d[7] = (TestCase){.k = key_for_double(2.2), value_for_double(2.2)};
    d[8] = (TestCase){.k = key_for_double(3.3), value_for_string("three point three")};

    fs->num_test_cases = 9;
    return fs;
}

// to free the hashmap created by the hashmap_fixture after a test
static void hashmap_free(void * fixture) {
    FixtureStruct *fs = fixture;
    map_destroy(fs->map);
    free(fs);  // also frees the Flexible Array
}


// -----------------------------------------------------------------
//      Dummy policies for test_create() functions
// -----------------------------------------------------------------


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

static void put_N_unique_strings(HashMap *map, unsigned num_strings) {
    //put num_strings unique strigs into the argument HashMap.
    for (int i = 0; i < num_strings; ++i) {
        char strkey[15]; // max 7 chars for value of i, plus 5 for 'hello', plus terminator = 13
        snprintf(strkey, 15, "hello%d",i);
        map_put( map, strkey, i );
        ColValue v = map_get(map, strkey );
        munit_assert_int( i, ==, v.vlong);
    }
}

static void put_test_data(HashMap *map, const size_t num_test_cases, TestCase cases[num_test_cases])  {
    //put num_strings unique strigs into the argument HashMap.
    for (int i = 0; i < num_test_cases; ++i) {
        (map_put)( map, cases[i].k, cases[i].v );
        ColValue v = (map_get)(map, cases[i].k );
        munit_assert_true(col_equals_ColValue(cases[i].v, v));
    }

}

// -------------------------------------------------
// test cases
// -------------------------------------------------

static MunitResult test_create_0(const MunitParameter params[], void* fixture) {
    HashMap *map = map_create();
    munit_assert_ptr_not_null(map);

    munit_assert_int(16, ==, map->num_buckets);
    munit_assert_true(map_equals_MapDataPolicies(MAP_DEFAULT_DATA_POLICIES, map->data_policies));
    munit_assert_true(mem_equals_MemPolicy(MEM_DEFAULT_MALLOC_POLICY, map->mem_policy));
    munit_assert_int(0, ==, map->size);
    map_destroy(map);
    return MUNIT_OK;
}

static MunitResult test_create_1(const MunitParameter params[], void* fixture) {
    HashMap *map = map_create(100);
    munit_assert_ptr_not_null(map);

    munit_assert_int(256, ==, map->num_buckets);
    munit_assert_true(map_equals_MapDataPolicies(MAP_DEFAULT_DATA_POLICIES, map->data_policies));
    munit_assert_true(mem_equals_MemPolicy(MEM_DEFAULT_MALLOC_POLICY, map->mem_policy));
    munit_assert_int(0, ==, map->size);
    map_destroy(map);
    return MUNIT_OK;
}

static MunitResult test_create_2(const MunitParameter params[], void* fixture) {
    // test passing num_buckets,  MapDataPolicies argument, default MemPolicy
    const MapDataPolicies data_policy = data_policy_fixture();
    HashMap *map = map_create(25, data_policy);
    munit_assert_ptr_not_null(map);

    munit_assert_int(64, ==, map->num_buckets);
    munit_assert_true(map_equals_MapDataPolicies(data_policy, map->data_policies));
    munit_assert_true(mem_equals_MemPolicy(MEM_DEFAULT_MALLOC_POLICY, map->mem_policy));
    munit_assert_int(0, ==, map->size);
    map_destroy(map);
    return MUNIT_OK;
}

static MunitResult test_create_3(const MunitParameter params[], void* fixture) {
    const MapDataPolicies data_policy = data_policy_fixture();
    const MemPolicy       mem_policy  = mem_policy_fixture();

    HashMap *map = map_create(25, data_policy, mem_policy);
    munit_assert_ptr_not_null(map);
    munit_assert_int(64, ==, map->num_buckets); //next power of two >= 25 is 32
    munit_assert_true(map_equals_MapDataPolicies(data_policy, map->data_policies));
    munit_assert_true(mem_equals_MemPolicy(mem_policy, map->mem_policy));
    munit_assert_int(0, ==, map->size);
    map_destroy(map);
    return MUNIT_OK;
}

static MunitResult test_create_max_size(const MunitParameter params[], void* fixture) {
    //try allocating MAX_CAPACITY * 2. Should clamp to MAX_CAPACITY
    HashMap *map = map_create(MAX_CAPACITY*2);
    munit_assert_ptr_not_null(map);

    munit_assert_ulong(MAX_NUM_BUCKETS, ==, map->num_buckets);
    munit_assert_true(map_equals_MapDataPolicies(MAP_DEFAULT_DATA_POLICIES, map->data_policies));
    munit_assert_true(mem_equals_MemPolicy(MEM_DEFAULT_MALLOC_POLICY, map->mem_policy));
    munit_assert_int(0, ==, map->size);
    map_destroy(map);
    return MUNIT_OK;
}



static MunitResult test_clear(const MunitParameter params[], void* fixture) {
    constexpr unsigned N = 100;
    FixtureStruct *fs = fixture;
    HashMap *map = fs->map;
    munit_assert_ptr_not_null(map);
    munit_assert_int(0, ==, map->size);
    munit_assert_ulong(16, ==, map->num_buckets);

    put_N_unique_strings(map, N);

    munit_assert_ulong(N, ==, map->size);
    munit_assert_ulong(map->size / map->load, ==, map->num_buckets);

    map_clear(map);
    munit_assert_int(0, ==, map->size);
    munit_assert_int(0, ==, map->load);
    munit_assert_ulong(256, ==, map->num_buckets);

    return MUNIT_OK;
}

static MunitResult test_contains_key(const MunitParameter params[], void* fixture) {
    FixtureStruct *fs = fixture;
    HashMap *map = fs->map;
    TestCase *cases = fs->test_data;
    unsigned num_test_cases = fs->num_test_cases;

    put_test_data(map, num_test_cases, cases);

    munit_assert_int(num_test_cases, ==, map->size);

    for (int i=0; i < num_test_cases; ++i ) {
        munit_assert_true( (map_contains_key)(map, cases[i].k));
    }
    return MUNIT_OK;
}

static MunitResult test_contains_value(const MunitParameter params[], void* fixture) {
    FixtureStruct *fs = fixture;
    HashMap *map = fs->map;
    TestCase *cases = fs->test_data;
    unsigned num_test_cases = fs->num_test_cases;

    put_test_data(map, num_test_cases, cases);

    munit_assert_int(num_test_cases, ==, map->size);

    for (int i=0; i < num_test_cases; ++i ) {
        munit_assert_true( (map_contains_value)(map, cases[i].v));
    }
    return MUNIT_OK;
}

static MunitResult test_put_get(const MunitParameter params[], void* fixture) {
    FixtureStruct *fs = fixture;
    HashMap *map = fs->map;
    TestCase *cases = fs->test_data;
    unsigned num_test_cases = fs->num_test_cases;
    munit_assert_ptr_not_null(map);
    munit_assert_int(0, ==, map->size);
    munit_assert_int(0, ==, map->load);

    put_test_data(map, num_test_cases, cases);

    munit_assert_int(num_test_cases, ==, map->size);
    munit_assert_double( (double)map->size / map->num_buckets, ==, map->load);
    for (int i=0; i < num_test_cases; ++i ) {
        ColValue v = (map_get)(map, cases[i].k);
        munit_assert_true(col_equals_ColValue(cases[i].v, v));
    }
    return MUNIT_OK;
}

static MunitResult test_get_or(const MunitParameter params[], void* fixture) {
    FixtureStruct *fs = fixture;
    HashMap *map = fs->map;
    TestCase *cases = fs->test_data;
    unsigned num_test_cases = fs->num_test_cases;
    MapKey missing = key_for_long(INT64_MIN);  // key is not in map
    munit_assert_int(0, ==, map->size);

    put_test_data(map, num_test_cases, cases);

    munit_assert_int(num_test_cases, ==, map->size);
    for (int i=0; i < num_test_cases; ++i ) {
        ColValue or_value = (map_get)(map, cases[i].k);
        munit_assert_true(col_equals_ColValue(cases[i].v, or_value));
        ColValue v = (map_get_or)(map, cases[i].k, or_value);
        munit_assert_true(col_equals_ColValue(cases[i].v, v ));
        v = (map_get_or)(map, missing, or_value);  // key is not in map
        munit_assert_true(col_equals_ColValue(or_value, v ));

    }
    return MUNIT_OK;
}

static MunitResult test_try_get(const MunitParameter params[], void* fixture) {
    FixtureStruct *fs = fixture;
    HashMap *map = fs->map;
    TestCase *cases = fs->test_data;
    unsigned num_test_cases = fs->num_test_cases;
    MapKey missing = key_for_long(INT64_MIN);  // key is not in map

    put_test_data(map, num_test_cases, cases);

    munit_assert_int(num_test_cases, ==, map->size);
    for (int i=0; i < num_test_cases; ++i ) {
        ColValue out_value;
        munit_assert_true( (map_try_get)(map,cases[i].k, &out_value ) );
        munit_assert_true(col_equals_ColValue(cases[i].v, out_value ));
        munit_assert_false( (map_try_get)(map, missing, &out_value ) );
        munit_assert_true(col_equals_ColValue(NULL_COL_VALUE, out_value ));
    }
    return MUNIT_OK;
}

static MunitResult test_is_empty(const MunitParameter params[], void* fixture) {
    FixtureStruct *fs = fixture;
    HashMap *map = fs->map;
    TestCase *cases = fs->test_data;
    unsigned num_test_cases = fs->num_test_cases;
    munit_assert_int(0, ==, map->size);
    munit_assert_true(map_is_empty(map));

    put_test_data(map, num_test_cases, cases);

    munit_assert_int(num_test_cases, ==, map->size);
    munit_assert_false(map_is_empty(map));

    map_clear(map);

    munit_assert_int(0, ==, map->size);
    munit_assert_true(map_is_empty(map));

    return MUNIT_OK;
}

static MunitResult test_size(const MunitParameter params[], void* fixture) {
    FixtureStruct *fs = fixture;
    HashMap *map = fs->map;
    TestCase *cases = fs->test_data;
    unsigned num_test_cases = fs->num_test_cases;
    munit_assert_int(0, ==, map->size);

    for (unsigned i = 0; i < num_test_cases; ++i) {
        (map_put)( map, cases[i].k, cases[i].v );
        munit_assert_int(i+1, ==, map->size);
    }
    for (unsigned i = num_test_cases; i-- > 0; ) {
        (map_remove)( map, cases[i].k);
        munit_assert_int(i, ==, map->size);
    }

    return MUNIT_OK;
}

static MunitResult test_remove(const MunitParameter params[], void* fixture) {
    FixtureStruct *fs = fixture;
    HashMap *map = fs->map;
    TestCase *cases = fs->test_data;
    unsigned num_test_cases = fs->num_test_cases;
    munit_assert_int(0, ==, map->size);

    put_test_data(map, num_test_cases, cases);

    for (unsigned i = num_test_cases; i-- > 0; ) {
        (map_remove)( map, cases[i].k);
        munit_assert_int(i, ==, map->size);
        (map_remove)( map, cases[i].k);  // trying to remove item that doesn't exist
        munit_assert_int(i, ==, map->size);  // should not affect size
    }



    return MUNIT_OK;
}


static MunitResult test_put_1M_uniquestrings(const MunitParameter params[], void* fixture) {
    unsigned N = 1'000'000;
    FixtureStruct *fs = fixture;
    HashMap *map = fs->map;
    munit_assert_ptr_not_null(map);
    munit_assert_int(0, ==, map->size);

    put_N_unique_strings(map, N);

    munit_assert_ulong(N, ==, map->size);
    // 1M items / .75 fill factor = 1.333M buckets, next power of 2 = 2^21 = 2,097,152
    munit_assert_ulong(2'097'152, ==, map->num_buckets);

    return MUNIT_OK;
}

static MunitResult test_create_with_mempolicy(const MunitParameter params[], void* fixture) {
    FixtureStruct *fs = fixture;
    // test passing num_buckets, MapDataPolicies, and MemPolicy arguments
    // todo we need to instrument the mem pool so we can test it.

    const MapDataPolicies data_policy = MAP_DEFAULT_DATA_POLICIES;
    const MemPolicy       mem_policy = mem_create_default_allocator(0);

    HashMap *map;
    map = map_create(0, data_policy, mem_policy);
    munit_assert_ptr_not_null(map);

    munit_assert_int(16, ==, map->num_buckets);
    munit_assert_int(0, ==, map->size);
    munit_assert_true(map_equals_MapDataPolicies(data_policy, map->data_policies));
    munit_assert_true(mem_equals_MemPolicy(mem_policy, map->mem_policy));

    put_N_unique_strings(map, fs->num_test_cases); // writing to our allocator

    munit_assert_int(16, ==, map->num_buckets);
    munit_assert_int(fs->num_test_cases, ==, map->size);


    map_destroy(map);

    return MUNIT_OK;
}

// ------------------------------------
// end test cases
// ------------------------------------




// make
//
//  clang -std=c23 -Wall -Werror -o ./out/test_hashmap2.out test_hashmap2.c string_counter.c hashmap.c ../memory/memory_pool.c ../munit/munit.c
int main_test_hashmap2(int argc, char *argv[argc + 1]) {
    setlocale(LC_NUMERIC, "en_US.UTF-8");   // use user's system locale

    MunitTest tests[] = {
        munit_test(test_create_0),
        munit_test(test_create_1),
        munit_test(test_create_2),
        munit_test(test_create_3),
        munit_test(test_clear),
        munit_test(test_contains_key),
        munit_test(test_contains_value),
        munit_test(test_put_get),
        munit_test(test_get_or),
        munit_test(test_try_get),
        munit_test(test_is_empty),
        munit_test(test_remove),
        munit_test(test_size),

        munit_test(test_create_max_size),
        munit_test(test_put_1M_uniquestrings),
        munit_test(test_create_with_mempolicy),



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

    return result;

}

#ifdef TEST_HASHMAP
int main(int argc, char *argv[argc + 1]) {
    return main_test_hashmap2(argc, argv);

}
#endif
