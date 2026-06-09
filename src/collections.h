// collections.h
//
// Copyright (c) Rob Ross 2026. 
//
//
// Created 2026/03/10 13:44:28 PDT

//
// Common objects shared by collection classes (HashMap, List)
//

#pragma once

#include "ref_objects.h"
#include "memory/memory_pool.h"

#include <stddef.h>
#include <stdint.h>




// 1 byte
typedef enum ColTypeEnum: unsigned char {
    COL_TYPE_NONE,
    COL_TYPE_LONG,
    COL_TYPE_DOUBLE,
    COL_TYPE_STRING,
    COL_TYPE_REF_OBJECT,
    COL_TYPE_VOID_PTR,
    COL_TYPE_NULL
} ColTypeEnum;



// 16 bytes
typedef struct ColValue {
    union {
        long        vlong;
        double      vdouble;
        char        *vstring;
        RefObject   *vref_object;
        void        *vvoid_ptr;
    };
    ColTypeEnum  value_type;
} ColValue;


//ColValuePolicyType: describes the policy for handling values added to a collection.
// 1 byte
typedef enum ColValuePolicyType: unsigned char {
    // default ininitialized value
    COL_VALUE_POLICY_NONE,
    // Collection makes a copy and owns the copy. Collection frees owned copy
    COL_VALUE_POLICY_COPY,
    // Collection takes ownership and does not make a copy. Collection frees. Caller must not free.
    COL_VALUE_POLICY_TAKE,
    // Collection uses value, does not copy,  does not free. Caller must not free while in use by Collection.
    //todo must implement reference counting for shared policy
    COL_VALUE_POLICY_SHARED
} ColValuePolicyType;


//Generic value policy. Here as high-level documentation.
// for HashMap, StringCounter see MapValuePolicy
// for List see ListValuePolicy
// 40 bytes
typedef struct ColValuePolicy {
    // A context pointer to be passed to the policy functions.
    void* context;
    // Called when a value is added. Returns the value to be stored.
    // Can be used for copying, interning, or reference counting.
    ColValue (*on_set_value)(void * context, ColValue value, ColValuePolicyType value_policy, MemPolicy mem_policy );
    // Called when a value is freed.
    void (*on_free_value)(void *collection, ColValue value, ColValuePolicyType value_policy, MemPolicy mem_policy );
    // for any specialized cleanup of the context
    void (*on_free_context)(void* context);
    ColValuePolicyType policy_type;
} ColValuePolicy;






//// ------------------------------------------------------------
////
////    Return and Error WIP
////
//// ------------------------------------------------------------

typedef enum CollectionsError {
    COL_OK = 0,
    COL_ERR_NULL_ARG,
    COL_ERR_ZERO_CAP,
    COL_ERR_OVERFLOW,
    COL_ERR_OUT_OF_MEM,
    COL_ERR_EMPTY,
    COL_ERR_INDEX_OUT_OF_RANGE
} CollectionsError;




static constexpr ColValue NULL_COL_VALUE = (ColValue){ .vvoid_ptr = nullptr, .value_type = COL_TYPE_NULL};
static constexpr size_t MAX_POW2 = (SIZE_MAX >> 1) + 1;

bool col_equals_ColValue( ColValue v1, ColValue v2);
bool col_equals_double(double d1, double d2);
size_t col_next_power_of_two(size_t wanted_size, size_t min_size, size_t max_size);
ColValue col_policy_value_set_default(void * context, ColValue value, ColValuePolicyType value_policy, MemPolicy mem_policy );
void col_policy_value_free_default(void * context, ColValue value, ColValuePolicyType value_policy, MemPolicy mem_policy ) ;

//// -----------------------------------------------------
////    Converters for generic map function arguments
////
////    these convert expressions to a ColValue
//// -----------------------------------------------------

ColValue value_for_long(long v);
ColValue value_for_double(double v);
ColValue value_for_string(const char *v);
ColValue value_for_void_ptr(const void *v);

#define COL_VALUE(V) ( _Generic( (V), \
        float: value_for_double, \
        double: value_for_double, \
        long double: value_for_double, \
        char *: value_for_string,     const char *: value_for_string, \
        void *: value_for_void_ptr,   const void *: value_for_void_ptr, \
        default: value_for_long \
    ) (V) )