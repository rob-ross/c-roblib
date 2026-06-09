// collections.c
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/03/10 13:55:52 PDT

//
// Common objects shared by collection classes (HashMap, List)
//

#include "collections.h"

#include <math.h>
#include <string.h>

// -----------------------
// Value data_policies
// -----------------------

ColValue col_policy_value_set_default(void * context, ColValue value, ColValuePolicyType value_policy, MemPolicy mem_policy ) {
    // default add always makse a copy of a string key and we own it
    if ( value.value_type == COL_TYPE_STRING &&
        ( value_policy == COL_VALUE_POLICY_COPY || value_policy == COL_VALUE_POLICY_NONE )) {
        char *string_copy = mem_strdup(mem_policy, value.vstring);
        return (ColValue){.value_type = COL_TYPE_STRING, .vstring = string_copy};
    }
    if ( value.value_type == COL_TYPE_REF_OBJECT && value.vref_object->data.string.length > 7 &&
        ( value_policy == COL_VALUE_POLICY_COPY || value_policy == COL_VALUE_POLICY_NONE ) ) {
        char *string_copy = mem_strdup(mem_policy, value.vref_object->data.string.ptr);
        return (ColValue){.value_type = COL_TYPE_STRING, .vstring = string_copy};
    }
    if ( value.value_type == COL_TYPE_VOID_PTR ) {
        // invoke the pointer's add method?
        //todo deal with void* types
    }
    return value;
}

void col_policy_value_free_default(void * context, ColValue value, ColValuePolicyType value_policy, MemPolicy mem_policy ) {
    if ( (value.value_type == COL_TYPE_STRING || (value.value_type == COL_TYPE_REF_OBJECT && value.vref_object->data.string.length > 7 ) ) &&
        ( value_policy == COL_VALUE_POLICY_COPY || value_policy == COL_VALUE_POLICY_TAKE || value_policy == COL_VALUE_POLICY_NONE )) {
        mem_free_bytes(mem_policy, value.vstring);  //we own it.
    } else if ( value.value_type == COL_TYPE_VOID_PTR ) {
            // invoke the pointer's free method
            //todo deal with void* types
    }
}

bool col_equals_RefString(RefString s1, RefString s2) {
    if (s1.length != s2.length ) return false;
    if (s1.length < 8 ) {
        return ( strcmp( s1.buf, s2.buf )  == 0);
    }
    return ( strcmp(s1.ptr, s2.ptr) == 0 );
}

bool col_equals_ColValue(const ColValue v1, const ColValue v2) {
    if (v1.value_type != v2.value_type) return false;

    switch (v1.value_type) {
        case COL_TYPE_NONE:
            return false;
        case COL_TYPE_LONG:
            return v1.vlong == v2.vlong;
        case COL_TYPE_DOUBLE:
            return col_equals_double(v1.vdouble, v2.vdouble);
        case COL_TYPE_STRING: {
            if (v1.vstring == v2.vstring) return true;
            return strcmp(v1.vstring, v2.vstring) == 0;
        }
        case COL_TYPE_REF_OBJECT: {
            return col_equals_RefString(v1.vref_object->data.string, v2.vref_object->data.string);
        }
        case COL_TYPE_VOID_PTR:
            // this requires the caller to have defined an equal function for this blob.
            // todo implement
            return v1.vvoid_ptr == v2.vvoid_ptr;
        case COL_TYPE_NULL:
            return true; // both value_types are null
        default:
            return false;
    }
}

bool col_equals_double(double d1, double d2) {
    // Optional policy: NaNs are not equal to anything (including NaN)
    if (isnan(d1) || isnan(d2)) return false;

    // Make -0.0 and +0.0 compare equal (matches common hashing data_policies)
    if (d1 == 0.0) d1 = 0.0;
    if (d2 == 0.0) d2 = 0.0;

    uint64_t ua, ub;
    memcpy(&ua, &d1, sizeof ua);
    memcpy(&ub, &d2, sizeof ub);
    return ua == ub;
}

//min_size and max_size must be powers of 2.
size_t col_next_power_of_two(size_t wanted_size, const size_t min_size, const size_t max_size) {
    // Clamp lower bound
    if (wanted_size < min_size) return min_size;

    // Clamp upper bound
    if (wanted_size >= max_size) wanted_size = max_size - 1;
    // note: next largest power of two might be > max size, but this is acceptable since this fudging is
    // already actored into max_size

    // Round up to next power of two
    // algorithm:
    // -----------------------------
    // 1. Subtract 1 so exact powers of two don’t round up.
    // 2. Fill all bits below the highest 1 with 1s.
    // 3. Add 1.
    wanted_size--;  // clears the lowest set bit and turns all lower bits into 1.
    wanted_size |= wanted_size >> 1;
    wanted_size |= wanted_size >> 2;
    wanted_size |= wanted_size >> 4;
    wanted_size |= wanted_size >> 8;
    wanted_size |= wanted_size >> 16;
#if SIZE_MAX > 0xffffffff
    wanted_size |= wanted_size >> 32;
#endif
    return wanted_size + 1;
}


//// -----------------------------------------------------
////    Converters for generic map function arguments
////
////    these convert expressions to a ColValue
//// -----------------------------------------------------

ColValue value_for_long(const long v) {
    return (ColValue){.vlong = v, .value_type = COL_TYPE_LONG};
}

ColValue value_for_double(const double v) {
    return (ColValue){.vdouble = v, .value_type = COL_TYPE_DOUBLE};
}

ColValue value_for_string(const char * v) {
    return (ColValue){.vstring = (char*)v, .value_type = COL_TYPE_STRING};
}

ColValue value_for_void_ptr(const void * v) {
    return (ColValue){.vvoid_ptr = (void*)v, .value_type = COL_TYPE_VOID_PTR};
}