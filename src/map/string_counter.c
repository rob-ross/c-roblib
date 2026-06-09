// string_counter.c
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/03/03 18:22:02 PST

//
// Wraps and delegates to HashMap. Adds specialized functions to act as a StringCounter.
//
#include <stdio.h>
#include <stdlib.h>

#include "string_counter.h"
#include "hashmap_private.h"

//// ------------------------------------------------------------
////    Static/private/internal methods
//// ------------------------------------------------------------

// Opaque pointer type.
// The full definition is now private to this .c file.
struct StringCounter {
    HashMap *map;
};




//// ------------------------------------------------------------
////    Default data policy functions
//// ------------------------------------------------------------

// ------------------------------
// Key data_policies
// ------------------------------

static MapKey sct_policy_key_add_default(HashMap map[static 1], MapKey key) {
    // default add always make a copy of a string key and we own it
    char * string_copy = mem_strdup(map->mem_policy,key.kstring);
    return (MapKey){.key_type = COL_TYPE_STRING, .kstring = string_copy};
}

static void sct_policy_key_free_default(HashMap map[static 1], MapKey key) {
    // default-add always make a copy of a string key and we own it, so we must free it
    if (map->mem_policy.free) {
        map->mem_policy.free(map->mem_policy.context, key.kstring);
    } else {
        free(key.kstring);
    }

}

// -----------------------
// Value data_policies
// -----------------------
// static void sct_policy_value_free_default(HashMap map[static 1], ColValue value) {
//     // values are always a long scalar, they don't need to be freed
//     // no-op
// }
static ColValue map_policy_value_add_to_stringpool(HashMap map[static 1], ColValue value) {
    if (!map) return NULL_COL_VALUE;
    StringCounter *sct = map->data_policies.value_policy.context; // this should be the stringpool
    if (!sct) return NULL_COL_VALUE;

    ColValuePolicyType valuepolicy = map->data_policies.value_policy.policy_type;

    if ( value.value_type == COL_TYPE_STRING &&
        ( valuepolicy == COL_VALUE_POLICY_SHARED || valuepolicy == COL_VALUE_POLICY_COPY || valuepolicy == COL_VALUE_POLICY_NONE )) {
        const char *strref = sct_ref(sct, value.vstring);
        return (ColValue){ .value_type = COL_TYPE_STRING, .vstring = (char *)strref };
        }
    if ( value.value_type == COL_TYPE_VOID_PTR ) {
        // invoke the pointer's free method
        //todo deal with void* types
    }

    return value;
}
static void map_policy_value_free_unref_from_stringpool(HashMap map[static 1], ColValue value) {
    if (!map) return;
    StringCounter *sct = map->data_policies.value_policy.context; // this should be the stringpool
    if (!sct) return;

    ColValuePolicyType valuepolicy = map->data_policies.value_policy.policy_type;

    if ( value.value_type == COL_TYPE_STRING &&
        ( valuepolicy == COL_VALUE_POLICY_SHARED || valuepolicy == COL_VALUE_POLICY_COPY || valuepolicy == COL_VALUE_POLICY_NONE )) {
        sct_unref(sct, value.vstring);
        return;
        }

    if ( value.value_type == COL_TYPE_VOID_PTR ) {
        // invoke the pointer's free method
        //todo deal with void* types
    }
}

static void map_policy_value_free_context_stringpool(void* context) {
    StringCounter *sct = (StringCounter*)context;
    if (!sct) return;
    sct_destroy(sct);
}



const MapKeyPolicy   SCT_DEFAULT_KEY_POLICY = (MapKeyPolicy){
    .policy_type   = COL_VALUE_POLICY_COPY,
    .on_add_key    = sct_policy_key_add_default,
    .on_free_key   = sct_policy_key_free_default,
};

const MapValuePolicy SCT_DEFAULT_VALUE_POLICY = (MapValuePolicy){
    .policy_type     = COL_VALUE_POLICY_NONE,
    .on_set_value    = nullptr,
    .on_free_value   = nullptr,
};

const MapDataPolicies SCT_DEFAULT_DATA_POLICIES = (MapDataPolicies){
    .key_policy   = SCT_DEFAULT_KEY_POLICY,
    .value_policy = SCT_DEFAULT_VALUE_POLICY
};


const MapValuePolicy MAP_STRING_POOL_VALUE_POLICIES =  (MapValuePolicy){
    .policy_type     = COL_VALUE_POLICY_SHARED,
    .on_set_value    = map_policy_value_add_to_stringpool,
    .on_free_value   = map_policy_value_free_unref_from_stringpool,
    .on_free_context = map_policy_value_free_context_stringpool
};

//// -----------------------------------
//// End default data policy functions
//// -----------------------------------





//// ------------------------------------------------------------
////
////    Public API methods
////
//// ------------------------------------------------------------


StringCounter * (sct_create)(size_t num_buckets, MapDataPolicies data_policies, MemPolicy mem_policy) {
    HashMap *map = (map_create)(num_buckets, data_policies, mem_policy);
    if (!map) return nullptr;
    StringCounter *sct = mem_alloc_bytes(mem_policy, sizeof(StringCounter));
    if (!sct) {
        mem_free_bytes(mem_policy, map);
        return nullptr;
    }
    sct->map = map;
    return sct;
}


void sct_destroy(StringCounter sct[static 1]) {
    if (!sct->map)  return;
    HashMap *map = sct->map;
    sct->map = nullptr;

    // If we're using an allocator that we own, we just free the allocator. We don't need to free individual nodes
    //  or keys.
    if (map->mem_policy.free_context) {
        if (map->mem_policy.policy_type == MEM_POLICY_ALLOCATOR_OWN) {
            map->mem_policy.free_context(map->mem_policy.context);
        }
    } else {
        mem_free_bytes(map->mem_policy, sct);
        map_destroy(map);
    }
}


// deletes all entries and frees them, but does not reduce bucket size or free allocated bucket memory.
void sct_clear(StringCounter sct[static 1]) {
    if (!sct->map) {
        return;
    }
    HashMap *map = sct->map;
    map_clear(map);
}

bool sct_contains_key(StringCounter sct[static 1], const char strkey[static 1]) {
    if (!sct->map) return false;
    return (map_contains_key)(sct->map, key_for_string(strkey));
}

ColValue sct_get(const StringCounter sct[static 1], const MapKey key) {
    return (map_get)(sct->map, key);
}

// returns the count for the argument string. If the string does not exist in the map, returns 0.
long sct_get_count(const StringCounter sct[static 1], const char string[static 1]) {
    if (!sct->map) {
        return 0;
    }
    const ColValue mv = (map_get)(sct->map, key_for_string(string));
    return mv.vlong;
}

// Ensures the argument string exists in the string_pool.
// Returns a pointer to a const char* that is equal to the `string` argument. Each invocation
// with the same `string` characters will return the same pointer value and increment count by 1 for that string.
const char* sct_ref(StringCounter sct[static 1], char string[static 1]) {
    if (!sct->map) return nullptr;

    const MapKey key = (MapKey){.kstring = string, .key_type = COL_TYPE_STRING};
    MapNode * node = map_node_for(sct->map, key);
    if (!node) {
        // first time string is encountered
        sct_put(sct, string, 1);
        node = map_node_for(sct->map, key);
    } else {
        // string in map, increment ref count
        node->value.vlong++;
    }

    if ( !node ) {
        return nullptr;
    }

    return node->key.kstring;
}

//reduce key refcount by 1. When refcount == 0, removes the key from the map
//StringCounter can store negative counts. if count == -1, unref makes the count -2.
// only removes the key when count goes from 1 to 0.
void sct_unref(StringCounter sct[static 1], const char string[static 1] ) {
    if (!sct->map) return;

    const MapKey key = key_for_string(string);
    MapNode * node = map_node_for(sct->map, key);

    if (!node) return ; // key not in map

    if (node->value.vlong - 1 == 0 ) {
        sct_remove(sct, string);
    } else {
        node->value.vlong--; //decrement ref count
    }
}


bool sct_is_empty(StringCounter sct[static 1]) {
    return map_is_empty(sct->map);
}

void sct_put(StringCounter sct[static 1], const char string[static 1], const long value) {
    if ( !sct->map ) return;

    HashMap *map = sct->map;
    (map_put)(map, key_for_string(string), value_for_long(value));
}


void (sct_remove)(StringCounter sct[static 1], const char string[static 1] ) {
    if (!sct->map) return;

    HashMap *map = sct->map;

    (map_remove)(map, key_for_string(string));
}

size_t sct_size(StringCounter sct[static 1]) {
    return map_size(sct->map);
}


//// ---------------------------------------------
////  repr methods
//// ---------------------------------------------

void sct_repr_StringCounter(StringCounter sct[static 1], const bool verbose) {
    if (!sct) {
        printf("(StringCounter)nullptr");
        return;
    }
    map_repr_HashMap(sct->map, verbose, "StringCounter");
}