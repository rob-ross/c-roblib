// ref_objects.h
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/03/16 22:06:26 PDT

//
// Created by Rob Ross on 3/16/26.
//

#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct BasicBlob {
    size_t size;
    uint8_t *data;
} BasicBlob;

// our first "special" type

typedef struct RefString {
    size_t length;
    union {
        // Small String Optimization (SSO)
        char const *ptr;                 // For long strings (allocated)
        char const buf[sizeof(char *)];  // For short strings (embedded)
    };
} RefString;

typedef union RefObjectData {
    RefString string;
} RefObjectData;

typedef int     ref_compare_function(void const*, void const*);
typedef size_t  ref_hash_function(void const*);
typedef bool    ref_equals_function(void const*, void const*);

typedef struct RefObject {
    RefObjectData data;
    unsigned refcount;
    // we probably need a RefObjectType enum here.
    ref_equals_function  *equals;
    ref_hash_function    *hash;
    ref_compare_function *compare;
} RefObject;


