// test_set.c
//
// Copyright (c) Rob Ross 2026.
//
//

//
// Created by Rob Ross on 3/3/26.
//

#include <locale.h>

#include "set.h"
#include "../testing_utils.h"



bool mem_equals_MemPolicy(MemPolicy o1, MemPolicy o2) {
    if ( o1.policy_type == o2.policy_type && o1.context == o2.context && o1.alloc == o2.alloc &&
        o1.calloc == o2.calloc && o1.realloc == o2.realloc && o1.free == o2.free &&
        o1.free_context == o2.free_context) {
        return true;
        }

    return false;
}


int main(int argc, char *argv[]) {
    setlocale(LC_NUMERIC, "en_US.UTF-8");   // use user's system locale

    MunitTest tests[] = {
        munit_test(test_create_0),


        MUNIT_NULL_TEST,
    };

    apply_fixture(tests, list_fixture, list_free);

    [[maybe_unused]]
         MunitSuite suite = {
        "/test_set", /* name */
        tests, /* tests */
        nullptr, /* suites */
        1, /* iterations */
        MUNIT_SUITE_OPTION_NONE /* options */
      };


    int result = 0;
    result = munit_suite_main(&suite, nullptr, argc, argv);

    // test_remove(nullptr, list_fixture(nullptr, nullptr));


    return result;

}



#ifdef TEST_SET
int main(int argc, char *argv[argc + 1]) {
    return main_test_set(argc, argv);

}
#endif