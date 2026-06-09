// ref_objects.c
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/03/16 22:06:26 PDT

//
// Created by Rob Ross on 3/16/26.
//

#include "ref_objects.h"

#include <stdio.h>
#include <string.h>



struct RefObjectFunctions {
    ref_equals_function  *equals;
    ref_hash_function    *hash;
    ref_compare_function *compare;
};


constexpr size_t REF_MAX_SSO_LENGTH = sizeof(char *) - 1;

// Forward references
static bool    ref_equals_RefString(void const *o1, void const *o2);
static size_t  ref_hash_code_RefString(void const *o1) ;
static int     ref_compare_RefString(void const *o1, void const *o2) ;

// RefString methods


struct RefObjectFunctions const REF_REFSTRING_FUNCTIONS = {
    .equals = ref_equals_RefString, .hash = ref_hash_code_RefString, .compare = ref_compare_RefString
};

//djb2 hash algorithm, best/worst/average time: Theta(N))
static inline size_t ref_djb2_hash_string(const size_t length, const char str[restrict static length] ) {
    unsigned long hash = 5381; // A "magic" prime number

    for (size_t i = 0; i < length; ++i ) {
        const int c = str[i];
        // hash = (hash * 33) + c
        // This is a fast way to write it using bit shifts:
        hash = ((hash << 5) + hash) + c;
    }
    return (size_t)hash;
}

static bool ref_equals_RefString(void const *o1, void const *o2) {
    RefString const *rs1 = o1;
    RefString const *rs2 = o2;

    if ( rs1 == rs2 ) return true;
    if ( !rs1 || !rs2 ) return false;

    if ( rs1->length != rs2->length ) return false;

    if ( rs1->length < REF_MAX_SSO_LENGTH) {
        // SSO string in buf member
        return memcmp(rs1->buf, rs2->buf, rs1->length) == 0;
    }
    return memcmp( rs1->ptr, rs2->ptr, rs1->length) == 0;
}

static size_t ref_hash_code_RefString(void const *o1) {
    RefString const *rs1 = o1;

    const char *p1 = (rs1->length < REF_MAX_SSO_LENGTH) ? rs1->buf : rs1->ptr;
    return ref_djb2_hash_string(rs1->length, p1);
}

static int ref_compare_RefString(void const *o1, void const *o2) {
    RefString const *rs1 = o1;
    RefString const *rs2 = o2;
    if ( rs1 == rs2 ) return 0;
    if ( !rs1) return -1; // null sorts first
    if ( !rs2) return  1;

    // Determine the correct pointer for each string (SSO buffer or heap pointer).
    // The type is `const char *` because both the `buf` array and `ptr` pointer
    // resolve to a pointer to constant character data.
    const char *p1 = (rs1->length < REF_MAX_SSO_LENGTH) ? rs1->buf : rs1->ptr;
    const char *p2 = (rs2->length < REF_MAX_SSO_LENGTH) ? rs2->buf : rs2->ptr;

    // Compare the common prefix of both strings.
    const size_t min_len = rs1->length < rs2->length ? rs1->length : rs2->length;
    const int cmp = memcmp(p1, p2, min_len);

    if (cmp != 0) {
        return cmp; // A difference was found in the common part.
    }

    // If the common prefix is identical, the result is determined by length.
    // A shorter string is "less than" a longer one (e.g., "cat" < "cattle").
    if (rs1->length < rs2->length) return -1;
    if (rs1->length > rs2->length) return 1;

    return 0; // Lengths are equal, strings are identical.
}
