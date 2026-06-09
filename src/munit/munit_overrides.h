//
// Created by Rob Ross on 1/29/26.
//
// overrides to standard munit macros to work with bootdev test runner.


#ifndef CS50X_MUNIT_OVERRIDES_H
#define CS50X_MUNIT_OVERRIDES_H

#include "munit.h"



#define munit_case(a, b, ...) \
MunitResult b(const MunitParameter params[], void* user_data_or_fixture) {\
    do {                \
       __VA_ARGS__      \
    } while (0);        \
    return MUNIT_OK;    \
}



// allows argument overloading of munit_test entries in MunitTest tests[] initializers

#define munit_test_1(n)                 (MunitTest){ .name= "/"#n, .test=(n) }
#define munit_test_2(n, t)              (MunitTest){ .name=n, .test=t }
#define munit_test_3(n, t, s)           (MunitTest){ .name=n, .test=t, .setup=s}
#define munit_test_4(n, t, s, td)       (MunitTest){ .name=n, .test=t, .setup=s, .tear_down=td}
#define munit_test_5(n, t, s, td, o)    (MunitTest){ .name=n, .test=t, .setup=s, .tear_down=td, .options=o}
#define munit_test_6(n, t, s, td, o, p) (MunitTest){ .name=n, .test=t, .setup=s, .tear_down=td, .options=o, .parameters=p}
#define munit_test_select(_1, _2, _3, _4, _5, _6, NAME, ... ) NAME
#define munit_test(...) \
    munit_test_select(__VA_ARGS__, munit_test_6, munit_test_5, \
        munit_test_4, munit_test_3, munit_test_2, munit_test_1 )(__VA_ARGS__)

#define munit_null_test { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }

#define munit_suite( a, b) (MunitSuite){ .prefix=a, .tests=b, .suites=NULL, .iterations=1, .options=MUNIT_SUITE_OPTION_NONE }


#endif //CS50X_MUNIT_OVERRIDES_H