

#pragma once
#include <stdio.h>
#include <inttypes.h>

#include "punit.h"




#define assertTrue_1(expr) \
    do { \
        if (!(expr)) { \
            punit_error("assertion failed: " #expr " is not true."); \
        } \
    } while (0)

#define assertTrue_2(expr, msg) \
    do { \
        if (!(expr)) { \
            punit_errorf("assertion failed: " #expr " is not true. %s", (msg)); \
        } \
    } while (0)

#define assertTrue_SELECT(_1, _2, NAME, ...) NAME
#define assertTrue(...) assertTrue_SELECT(__VA_ARGS__, assertTrue_2, assertTrue_1)(__VA_ARGS__)



#define PUNIT_ASSERT_TRUE_BODY(expr, fail_call) \
    do { \
        if (!PUNIT_LIKELY(expr)) { \
            fail_call; \
        } \
    PUNIT_PUSH_DISABLE_MSVC_C4127_ \
    } while (0) \
    PUNIT_POP_DISABLE_MSVC_C4127_

#define punit_assertTrue_1(expr) \
PUNIT_ASSERT_TRUE_BODY((expr), punit_error("assertion failed: " #expr " is not true"))

#define punit_assertTrue_2(expr, msg) \
PUNIT_ASSERT_TRUE_BODY((expr), punit_errorf("assertion failed: " #expr " is not true. %s", (msg)))

#define punit_assertTrue_SELECT(_1, _2, NAME, ...) NAME
#define punit_assertTrue(...) punit_assertTrue_SELECT(__VA_ARGS__, punit_assertTrue_2, punit_assertTrue_1)(__VA_ARGS__)


// ------------------------
// assertEqual
// ------------------------
#define PUNIT_ASSERT_EQUAL_BODY(a, b, fail_call) \
    do { \
        if (!PUNIT_LIKELY( a == b )) { \
            fail_call; \
        } \
        PUNIT_PUSH_DISABLE_MSVC_C4127_ \
    } while (0) \
    PUNIT_POP_DISABLE_MSVC_C4127_

#define punit_assertEqual_1(a, b) \
PUNIT_ASSERT_EQUAL_BODY((a), (b), punit_error("assertion failed.\nExpected=%s\nActual=%s", somecall(a), somecall(b)))

#define punit_assertEqual_2(a, b, msg) \
PUNIT_ASSERT_EQUAL_BODY((a), (b), punit_errorf("assertion failed: %s\nExpected=%s\nActual=%s" , (msg)))

#define punit_assertEqual_SELECT(_1, _2, _3, NAME, ...) NAME
#define punit_assertEqual(...) punit_assertEqual_SELECT(__VA_ARGS__, punit_assertEqual_2, punit_assertEqual_1)(__VA_ARGS__)


// ------------------------
// assertNotEqual
// ------------------------
#define PUNIT_ASSERT_NOT_EQUAL_BODY(a, b, fail_call) \
do { \
if (!PUNIT_LIKELY( a != b )) { \
fail_call; \
} \
PUNIT_PUSH_DISABLE_MSVC_C4127_ \
} while (0) \
PUNIT_POP_DISABLE_MSVC_C4127_

// AI add code below here:


/*
 *  Notes;
 *  -------
 *  we interpret `unsigned char*` as a pointer to raw bytes and format it as a pointer type. This should probably be
 *  configurable for a given unit test in case the user wants to use unsigned characters in strings.
 */
#define PUNIT_FMT(expr, ...) _Generic( (expr), \
_Bool: "%d", \
char: "%c", \
signed char: "%hhd", \
unsigned char: "%hhu", \
char*: "%s", \
unsigned char*: "%p", \
short: "%hd", \
unsigned short: "%hu", \
int: "%d", \
unsigned int: "%u", \
long: "%ld", \
unsigned long: "%lu", \
long long: "%lld", \
unsigned long long: "%llu", \
float: "%f", \
double: "%f", \
long double: "%Lf", \
void*: "%p", \
const void*: "%p", \
default: "%p" \
)

// capture compile time type of expressions
// volatile and _Atomic modifiers are currently not supported. Their inclusion would add 7 more entries for each pointer case
// in this list.
#define PUNIT_TYPE(expr, ...) _Generic( (expr), \
_Bool: "_Bool", \
char: "char", \
signed char: "signed char", \
unsigned char: "unsigned char", \
char*: "char*", \
unsigned char*: "unsigned char*", \
signed char*: "signed char*",\
const char*: "const char*", \
const unsigned char*: "const unsigned char*", \
const signed char*: "const signed char*",\
\
short: "short", \
unsigned short: "unsigned short", \
short*: "short*",\
unsigned short*: "unsigned short*",\
const short*: "const short*",\
const unsigned short*: "const unsigned short*",\
\
int: "int", \
unsigned int: "unsigned int", \
int*: "int*", \
unsigned int*: "unsigned int*", \
const int*: "const int*", \
const unsigned int*: "const unsigned int*", \
\
long: "long", \
unsigned long: "unsigned long", \
long*: "long*", \
unsigned long*: "unsigned long*", \
const long*: "const long*", \
const unsigned long*: "const unsigned long*", \
\
long long: "long long", \
unsigned long long: "unsigned long long", \
long long*: "long long*", \
unsigned long long*: "unsigned long long*", \
const long long*: "const long long*", \
const unsigned long long*: "const unsigned long long*", \
\
float: "float", \
float*: "float*", \
const float*: "const float*", \
double: "double", \
double*: "double*", \
const double*: "const double*", \
\
long double: "long double", \
long double*: "long double*", \
const long double*: "const long double*", \
void*: "void*", \
const void*: "const void*", \
default: "unknown" \
)