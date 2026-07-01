//
// Created by Rob Ross on 2/21/26.
//

#pragma once

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
    // If not C23, include the header that provides 'bool'
    #include <stdbool.h>
#endif

// 1. Check if we are in C23
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    // 2. Check for known-good library versions
    #if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 39))
        // Linux with glibc 2.39+
        #define BOOL_FMT "%b"
    #elif defined(__APPLE__)
        // Currently, even macOS 15.2 doesn't reliably support %b in printf.
        // It is safer to use %d for now.
        #define BOOL_FMT "%d"
    #else
        // Fallback for unknown C23 environments
        #define BOOL_FMT "%d"
    #endif

#else
    // Pre-C23
    #include <stdbool.h>
    #define BOOL_FMT "%d"
#endif

// ----------------------------
// HELPER MACROS
// ----------------------------

#define STATEMENT(S) do{ S }while(0)
#define STRINGIFY_(S) #S
#define STRINGIFY(S) STRINGIFY_(S)
#define CAT_(A, B) A##B
#define CAT(A, B) CAT_(A, B)
#define CAT3(A,B,C) CAT(CAT(A,B), C)
#define ARRAY_COUNT(a) (sizeof(a)/sizeof(*(a)))
#define INT_FROM_PTR(p) (unsigned long long)((char*)p - (char*)0)
#define PTR_FROM_INT(i) (void*)((char*)0 + (i))
#define MEMBER(T, m) (((T*)0)->m)
#define OFFSET_OF_MEMEBER(T, m) INT_FROM_PTR(&MEMBER(T, m))
#define MIN(a, b) (((a)<(b))?(a):(b))
#define MAX(a, b) (((a)>(b))?(a):(b))
#define CLAMP(a, x, b) (((x)<(a))?(a):((b)<(x))?(b):(x))
#define CLAMP_TOP(a, b) MIN(a, b)
#define CLAMP_BOT(a, b) MAX(a, b)

#include <string.h>
#define MEM_ZERO(p, z) memset((p), 0, (z))
#define MEM_ZERO_STRUCT(p) MEM_ZERO((p), sizeof(*(p)))
#define MEM_ZERO_ARRAY(p) MEM_ZERO((p), sizeof(p))
#define MEM_ZERO_TYPED(p, c) MEM_ZERO((p), sizeof(*(p)) *(c))
#define MEM_MATCH(a, b, z)  (memcmp((a), (b), (z)) == 0 )
#define MEM_COPY(d, s, z) memmove((d), (s), (z))
#define MEM_COPY_STRUCT(d, s) MEM_COPY((d), (s), MIN(sizeof(*(d), sizeof(*(s)))))
#define MEM_COPY_ARRAY(d, s) MEM_COPY((d), (s), MIN(sizeof(d), sizeof(s)))
#define MEM_COPY_TYPED(d, s, c) MEM_COPY((d), (s), MIN(sizeof(*(d)), sizeof(*(s))) * (c) )


// Helper to select format specifier based on type (C11)
// remember this is a *compiler* time macro, not preprocessor
#define IS_BOOL(x) _Generic((x), bool: 1, default: 0)
#define IS_ARRAY(x) (!(_Generic((x), typeof(x): 0, typeof(&(x)[0]): 1, default: 0)))
#define IS_INT_ARRAY(x) (sizeof(x) > 0 && _Generic((x), int*: 0, default: 1))  // Rough example

#define CONVERSION_FMT(expr, ...) _Generic( (expr),    \
    bool                      : "%d",                  \
    bool*                     : "%p",                  \
    bool const*               : "%p",                  \
    char                      : "%hhd",                \
    char*                     : "%p",                  \
    char const*               : "%p",                  \
    unsigned char             : "%hhu",                \
    unsigned char*            : "%p",                  \
    unsigned char const*      : "%p",                  \
    signed char               : "%hhd",                \
    signed char*              : "%p",                  \
    signed char const*        : "%p",                  \
    short                     : "%hd",                 \
    short*                    : "%p",                  \
    short const*              : "%p",                  \
    unsigned short            : "%hu",                 \
    unsigned short*           : "%p",                  \
    unsigned short const*     : "%p",                  \
    int                       : "%d",                  \
    int*                      : "%p",                  \
    int const*                : "%p",                  \
    unsigned int              : "%u",                  \
    unsigned int*             : "%p",                  \
    unsigned int const*       : "%p",                  \
    long                      : "%ld",                 \
    long*                     : "%p",                  \
    long const*               : "%p",                  \
    unsigned long             : "%lu",                 \
    unsigned long*            : "%p",                  \
    unsigned long const*      : "%p",                  \
    long long                 : "%lld",                \
    long long*                : "%p",                  \
    long long const*          : "%p",                  \
    unsigned long long        : "%llu",                \
    unsigned long long*       : "%p",                  \
    unsigned long long const* : "%p",                  \
    float                     : "%f",                  \
    float*                    : "%p",                  \
    float const*              : "%p",                  \
    double                    : "%f",                  \
    double*                   : "%p",                  \
    double const*             : "%p",                  \
    long double               : "%Lf",                 \
    long double*              : "%p",                  \
    long double const*        : "%p",                  \
    void*                     : "%p",                  \
    void const*               : "%p",                  \
    default                   : "%p"                  \
)


// C11 _Generic selection to mimic a typeof() string for inspection
#define TYPE_STR(expr, ...) _Generic( (expr),    \
    bool                      : "bool",                        \
    bool*                     : "bool*",                       \
    bool const*               : "bool const*",                 \
    char                      : "char",                        \
    char*                     : "char*",                       \
    char const*               : "char const*",                 \
    unsigned char             : "unsigned char",               \
    unsigned char*            : "unsigned char*",              \
    unsigned char const*      : "unsigned char const*",        \
    signed char               : "signed char",                 \
    signed char*              : "signed char*",                \
    signed char const*        : "signed char const*",          \
    short                     : "short",                       \
    short*                    : "short*",                      \
    short const*              : "short const*",                \
    unsigned short            : "unsigned short",              \
    unsigned short*           : "unsigned short*",             \
    unsigned short const*     : "unsigned short const*",       \
    int                       : "int",                         \
    int*                      : "int*",                        \
    int const*                : "int const*",                  \
    unsigned int              : "unsigned int",                \
    unsigned int*             : "unsigned int*",               \
    unsigned int const*       : "unsigned int const*",         \
    long                      : "long",                        \
    long*                     : "long*",                       \
    long const*               : "long const*",                 \
    unsigned long             : "unsigned long",               \
    unsigned long*            : "unsigned long*",              \
    unsigned long const*      : "unsigned long const*",        \
    long long                 : "long long",                   \
    long long*                : "long long*",                  \
    long long const*          : "long long const*",            \
    unsigned long long        : "unsigned long long",          \
    unsigned long long*       : "unsigned long long*",         \
    unsigned long long const* : "unsigned long long const*",   \
    float                     : "float",                       \
    float*                    : "float*",                      \
    float const*              : "float const*",                \
    double                    : "double",                      \
    double*                   : "double*",                     \
    double const*             : "double const*",               \
    long double               : "long double",                 \
    long double*              : "long double*",                \
    long double const*        : "long double const*",          \
    void*                     : "void*",                       \
    void const*               : "void const*",                 \
    default                   : "unknown"                      \
)

/**
 * Prints a single new line
 */
#define PL printf("\n")

/**
 * Prints value of argument with no newline
 * @param x scalar value, pointer, or expression. Not tested for arrays, structs, unions, or enums
 */
#define PV(x) STATEMENT( \
    printf(" %s == (%s)", #x, TYPE_STR(x)); \
    if ( IS_BOOL(x) ){ \
        printf( (x) == 0 ? "false" : "true");\
    } else {    \
        printf(CONVERSION_FMT(x), (x)); \
    }   \
    printf(","); \
)

/**
 * Prints value of argument with a newline
 * @param x scalar value, pointer, or expression. Not tested for arrays, structs, unions, or enums
 */
#define PVL(x) STATEMENT( PV(x); printf("\n"); )

/**
 * Prints the argument string with a newline
 * @param s string value
 */
#define PS(s) STATEMENT( printf("%s == \"%s\"\n", #s, (s) ); )


// expands to printf( fmt, ...)  but adds a newline to the end of fmt.
#define print(fmt, ...)                 \
    do {                                \
        printf((fmt), ##__VA_ARGS__);   \
        putchar('\n');                  \
    } while (0)

