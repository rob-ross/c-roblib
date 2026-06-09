//
// Created by Rob Ross on 2/6/26.
//

#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <setjmp.h>

// ----------------------------
// HELPER MACROS
// ----------------------------

#define STATEMENT(S) do{ S }while(0)


#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  define PUNIT_NO_RETURN _Noreturn
#elif defined(__GNUC__)
#  define PUNIT_NO_RETURN __attribute__((__noreturn__))
#else
#  define PUNIT_NO_RETURN
#endif

// -------------------------------------------------------
// PUNIT_TAG: test metadata annotation for tooling
// -------------------------------------------------------
#if defined(__clang__)
  #define PUNIT_TAG(payload) __attribute__((annotate("" payload "")))
#elif defined(__GNUC__)
  #define PUNIT_TAG(payload) __attribute__((annotate("" payload "")))
#else
  // Fallback for non-attribute compilers; parser can read this comment.
  #define PUNIT_TAG(payload) /* PUNIT_TAG "" payload "" */
#endif



#if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__SUNPRO_CC) || defined(__IBMCPP__)
#  define PUNIT_THREAD_LOCAL __thread
#elif (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201102L)) || defined(_Thread_local)
#  define PUNIT_THREAD_LOCAL _Thread_local
#else
#  define PUNIT_THREAD_LOCAL
#endif

typedef enum {
    PUNIT_LOG_DEBUG,
    PUNIT_LOG_INFO,
    PUNIT_LOG_WARNING,
    PUNIT_LOG_ERROR
  } PunitLogLevel;

#if defined(__GNUC__) && !defined(__MINGW32__)
#  define PUNIT_PRINTF(string_index, first_to_check) __attribute__((format (printf, string_index, first_to_check)))
#else
#  define PUNIT_PRINTF(string_index, first_to_check)
#endif

// Pointer to the currently active jump buffer (stack)
// We use extern so test_unit.c can see the variable defined in unit.c
#if defined(PUNIT_THREAD_LOCAL)
extern PUNIT_THREAD_LOCAL jmp_buf *punit_active_jmp_buf;
extern PUNIT_THREAD_LOCAL bool punit_capture_asserts;

#define assertThrows(expr) \
    do { \
        jmp_buf local_env; \
        jmp_buf *old_env = punit_active_jmp_buf; \
        bool old_capture = punit_capture_asserts; \
        punit_active_jmp_buf = &local_env; \
        punit_capture_asserts = true; \
        if (setjmp(local_env) == 0) { \
            expr; \
            punit_active_jmp_buf = old_env; \
            punit_capture_asserts = old_capture; \
            punit_errorf("Expected exception in '%s', but none was thrown.", #expr); \
        } else { \
            punit_active_jmp_buf = old_env; \
            punit_capture_asserts = old_capture; \
        } \
    } while (0)
#endif

PUNIT_PRINTF(5, 6)
void punit_logf_ex(PunitLogLevel level, const char* filename, int line, const char* func, const char* format, ...);

#define punit_logf(level, format, ...) \
punit_logf_ex(level, __FILE__, __LINE__, __func__, format, __VA_ARGS__)

#define punit_log(level, msg) \
punit_logf(level, "%s", msg)

PUNIT_NO_RETURN
PUNIT_PRINTF(4, 5)
void punit_errorf_ex(const char* filename, int line, const char* func, const char* format, ...);

#define punit_errorf(format, ...) \
    punit_errorf_ex(__FILE__, __LINE__, __func__, format, __VA_ARGS__)

#define punit_error(msg) \
    punit_errorf("%s", msg)


// -----------
// assertTrue
// -----------

#define assertTrue_1(expr) \
    STATEMENT(          \
        if (!(expr)) {  \
            punit_error("assertion failed: " #expr " is not true."); \
        }               \
    )

#define assertTrue_2(expr, msg) \
    STATEMENT(\
        if (!(expr)) { \
            punit_errorf("assertion failed: " #expr " is not true. %s", (msg)); \
        } \
    )

#define assertTrue_SELECT(_1, _2, NAME, ...) NAME
#define assertTrue(...) assertTrue_SELECT(__VA_ARGS__, assertTrue_2, assertTrue_1)(__VA_ARGS__)

// -----------
// assertFalse
// -----------

#define assertFalse_1(expr) \
STATEMENT( \
        if ( (expr) ) { \
            punit_error("assertion failed: " #expr " is not false."); \
        } \
    )

#define assertFalse_2(expr, msg) \
    STATEMENT( \
            if ( (expr) ) { \
                punit_errorf("assertion failed: " #expr " is not false. %s", (msg)); \
            } \
    )

#define assertFalse_SELECT(_1, _2, NAME, ...) NAME
#define assertFalse(...) assertFalse_SELECT(__VA_ARGS__, assertFalse_2, assertFalse_1)(__VA_ARGS__)



// Helper to select format specifier based on type (C11)
#define PUNIT_FMT(expr, ...) _Generic( (expr), \
    char              : "%c",   \
    unsigned char     : "%hhu", \
    signed char       : "%hhd", \
    short             : "%hd",  \
    unsigned short    : "%hu",  \
    int               : "%d",   \
    unsigned int      : "%u",   \
    long              : "%ld",  \
    unsigned long     : "%lu",  \
    long long         : "%lld", \
    unsigned long long: "%llu", \
    float             : "%f",   \
    double            : "%f",   \
    long double       : "%Lf"   \
)

// ------------------------
// assertEqual
// ------------------------

#define PUNIT_ASSERT_EQUAL_BODY(a, b, fail_call) \
    STATEMENT( \
            if (!( a == b )) { \
                fail_call; \
            } \
    )


#define punit_assertEqual_1(a, b) \
PUNIT_ASSERT_EQUAL_BODY((a), (b), \
    punit_errorf( \
        insert_conversion_specifiers("assertion failed.\nExpected: %s \nActual  : %s", \
            PUNIT_FMT(a), PUNIT_FMT(b) ), (a), (b)))

#define punit_assertEqual_2(a, b, msg) \
PUNIT_ASSERT_EQUAL_BODY((a), (b), \
    punit_errorf(\
        insert_conversion_specifiers("assertion failed: %s\nExpected: %s\nActual  : %s", \
            "%s", PUNIT_FMT(a), PUNIT_FMT(b) ), (msg), (a), (b)))

#define punit_assertEqual_SELECT(_1, _2, _3, NAME, ...) NAME
#define assertEqual(...) \
    punit_assertEqual_SELECT(__VA_ARGS__, punit_assertEqual_2, punit_assertEqual_1)(__VA_ARGS__)



// ------------------------
// assertNotEqual
// ------------------------

#define PUNIT_ASSERT_NOT_EQUAL_BODY(a, b, fail_call) \
    STATEMENT( \
            if (!( a != b )) { \
                fail_call; \
            } \
    )


#define punit_assertNotEqual_1(a, b) \
    PUNIT_ASSERT_NOT_EQUAL_BODY((a), (b), \
        punit_errorf( \
            insert_conversion_specifiers("assertion failed.\nExpected: %s \nActual  : %s", \
                PUNIT_FMT(a), PUNIT_FMT(b) ), (a), (b) ) )

#define punit_assertNotEqual_2(a, b, msg) \
    PUNIT_ASSERT_NOT_EQUAL_BODY((a), (b), \
        punit_errorf(\
            insert_conversion_specifiers("assertion failed: %s\nExpected: %s\nActual  : %s", \
                "%s", PUNIT_FMT(a), PUNIT_FMT(b) ), (msg), (a), (b) ) )

#define punit_assertNotEqual_SELECT(_1, _2, _3, NAME, ...) NAME
#define assertNotEqual(...) \
    punit_assertNotEqual_SELECT(__VA_ARGS__, punit_assertNotEqual_2, punit_assertNotEqual_1)(__VA_ARGS__)

// -------------------------------------------------------
// tests
char *insert_conversion_specifiers(const char *format_str, ...);

typedef void (*test_case)(void);
void run_test(test_case test);
void test_runner(test_case tests[]);