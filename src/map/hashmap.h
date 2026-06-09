// hashmap.h
//
// Copyright (c) Rob Ross 2026.
//
//

#pragma once

#include <stdint.h> // For uint8_t

#include "../collections.h"
#include "../memory/memory_pool.h"



// Forward-declare dependency types.
typedef struct StringCounter StringCounter;
typedef struct HashMap HashMap;



// if we really intend to support *any* data buffer type as a key in the map, we need to be provided:
// 1. a free function pointer, if we need to free it.
// 2. ownership flags? Map owns it or caller owns it? Other values.... for char* maybe "is literal" as a guard to not free.
// 3. do we copy the blob or not? If we're going to own it without transfer of ownership, we'll have to make a copy.
//      if the caller is transfering ownership, we won't need to copy it.
// 4. hash function and is_equal method. Perhaps a compare as well.
// for now we won't support void* as a map key. We copy all strings and free the copies when we're done with them.

// 16 bytes
typedef struct MapKey {
    union {
        long klong;
        double kdouble;
        char *kstring;
        void *kvoid_ptr;
    };
    ColTypeEnum  key_type;
} MapKey;


// 48 bytes
typedef struct MapNode {
    const MapKey key;
    ColValue     value;
    const size_t hash;
    struct MapNode  *next;
} MapNode;


// 40 bytes
typedef struct MapKeyPolicy {
    // A context pointer to be passed to the policy functions.
    void* context;
    // Called when a key is added. Returns the key to be stored.
    // Can be used for copying, interning, or reference counting.
    MapKey (*on_add_key)(HashMap *map, MapKey key);
    // Called when a key is freed.
    void (*on_free_key)(HashMap *map, MapKey key);
    // for any specialized cleanup of the context
    void (*on_free_context)(void* context);
    ColValuePolicyType policy_type;
} MapKeyPolicy;

// 40 bytes
typedef struct MapValuePolicy {
    // A context pointer to be passed to the policy functions.
    void* context;
    // Called when a value is added. Returns the value to be stored.
    // Can be used for copying, interning, or reference counting.
    ColValue (*on_set_value)(HashMap *map, ColValue value);
    // Called when a value is freed.
    void (*on_free_value)(HashMap *map, ColValue value);
    // for any specialized cleanup of the context
    void (*on_free_context)(void* context);
    ColValuePolicyType policy_type;
} MapValuePolicy;

// 80 bytes
typedef struct MapDataPolicies {
    MapKeyPolicy   key_policy;
    MapValuePolicy value_policy;
} MapDataPolicies;


// 192 bytes
typedef struct HashMap {
    MapNode **buckets;            // each bucket is a linked list
    size_t size;                  // Number of key-value pairs currently in the map
    size_t fill_capacity;         // holds the max ideal size for the current number of buckets. Increases when buckets increase
    double load;                  // current load = size / num_buckets
    size_t num_buckets;           // must always be a power of 2
    double fill_factor;           // desired load

    MapDataPolicies data_policies;
    MemPolicy       mem_policy;
    uint64_t        flags; // future use
} HashMap;


//// ------------------------------------------------------------
////
////    Return and Error WIP
////
//// ------------------------------------------------------------
//
// typedef enum MapErrorCode: unsigned char {
//     MAP_OK,
//     MAP_ERR_NULL_ARG,
//     MAP_ERR_OUT_OF_MEM,
//
// } MapErrorCode;
//
// typedef struct MapError {
//     MapErrorCode err;
//     const char msg[]; // error message, flexible array
// } MapError;
//
// typedef struct MapResult {
//     union {
//         MapKey   key;
//         ColValue value;
//     };
//     const char type[1]; // holds either 'k' or 'v'
// } MapResult;
//
// typedef struct MapRE {
//     MapResult result;
//     MapError  err;
// } MapRE;

static constexpr double DEFAULT_FILL_FACTOR = 0.75;

// 2^28 = 268,435,456 buckets, capacity = 201,326,592, sizeof(all MapNodes) =  9.216 GB,  sizeof(buckets[]) = 2048 MB
// Ram for just Hashmap array and nodes = 11.264 GB. + storage for strings.
static constexpr size_t MIN_NUM_BUCKETS = 16;
static constexpr size_t MAX_NUM_BUCKETS = 268'435'456;
static constexpr size_t MIN_CAPACITY  = 12;
static constexpr size_t MAX_CAPACITY = (size_t)(MAX_NUM_BUCKETS * DEFAULT_FILL_FACTOR);  // currently 201,326,592

extern const MapKey          NULL_MAP_KEY;
extern const MapNode         NULL_MAP_NODE;
extern const MapDataPolicies MAP_DEFAULT_DATA_POLICIES;



// -------------------------------------
// 'Package-private/friend' API methods
//
// declared in hashmap_private.h
// -------------------------------------



//// ------------------------------------------------------------
////
////    Public API methods
////
//// ------------------------------------------------------------


// call map_destroy to free all resources
HashMap * (map_create)(unsigned initial_capacity, MapDataPolicies data_policies, MemPolicy mem_policy) ;
void map_destroy(HashMap map[static 1]);

// creates and returns a HashMap backed by a string pool that interns all string values. Good when the same string is
// used for multiple key values. Call map_destroy to free all allocated resources, including the backing string pool.
HashMap *map_create_using_stringpool(size_t num_buckets, const MemPolicy *mem_policy) ;

//Removes all the mappings from this map. Keeps existing buckets. After call, size == 0.
void map_clear(HashMap map[static 1]);
//  Returns true if this map contains a mapping for the specified key.
// if you intend to use the key's value immediately if it exists, consider using map_try_get instead, for efficiency.
bool map_contains_key(HashMap map[static 1], MapKey key) ;
//  Returns true if this map contains a mapping for the specified key. Currently O(N)
bool map_contains_value(HashMap map[static 1], ColValue value);


/**
 * Returns the value associated with the given key.
 *
 * If the key is not found, returns a ColValue where value_type is MAP_TYPE_NULL.
 *
 * Ownership (Borrowing):
 *  - For pointer types (e.g. strings), the caller "borrows" the pointer from the HashMap.
 *  - The caller must NOT free the returned pointer.
 *  - The pointer is valid only as long as the entry remains in the HashMap.
 *  - Removing the key from the HashMap invalidates the pointer (dangling pointer).
 */
ColValue (map_get)(const HashMap map[static 1], MapKey key);
// Returns the value to which the specified key is mapped, or fallback if this map contains no mapping for the key.
ColValue (map_get_or)(const HashMap map[static 1], MapKey key, ColValue fallback) ;
// if key exists, copies the value into out and returns true. If key does not exist,
// writes NULL_COL_VALUE to out and returns false
bool (map_try_get)(const HashMap map[static 1], MapKey key, ColValue *out);

// Returns true if this map contains no key-value mappings.
bool map_is_empty(const HashMap map[static 1]);

//associates value with key in the map. If key did not previously exist in the map, this
// function adds it. If the key already exists, the value is replaced with the argument value.
// If the key previously existed, the old value is returned. If the key is being added, returns
//NULL_COL_VALUE.
void map_put(HashMap map[static 1], MapKey key, ColValue value) ;

//Removes the mapping for a key from this map if it is present
// Returns the value to which this map previously associated the key, or null if the map contained no mapping for the key.
void (map_remove)(HashMap map[static 1], MapKey key);

// Returns the number of key-value mappings in this map.
size_t map_size(const HashMap *map);


//// ---------------------------------------------
////  repr methods
//// ---------------------------------------------
void map_repr_HashMap(const HashMap map[static 1], bool verbose, const char* type_str);
void map_repr_MapKey(MapKey map_key, bool verbose);
void map_repr_MapValue(ColValue map_value, bool verbose);
void map_repr_Node(const MapNode node[static 1]);



//// -----------------------------------------------------
////    Converters for generic map function arguments
////
////    these convert expressions to a MapKey
//// -----------------------------------------------------

MapKey key_for_long(long k);
MapKey key_for_double(double k);
MapKey key_for_string(const char *k);
MapKey key_for_void_ptr(const void *k);


// Macros for type-generic map functions
#define MAP_KEY(K) ( _Generic( (K), \
    float: key_for_double, \
    double: key_for_double, \
    long double: key_for_double, \
    char *: key_for_string,     const char *: key_for_string, \
    void *: key_for_void_ptr,   const void *: key_for_void_ptr, \
    default: key_for_long \
) (K) )


#define map_contains_key(M, K) map_contains_key( (M), MAP_KEY(K))
#define map_contains_value(M, V) map_contains_value( (M), COL_VALUE(V))
#define map_get(M, K) map_get( (M), MAP_KEY(K) )
#define map_get_or(M, K, V) map_get_or( (M), MAP_KEY(K), COL_VALUE(V) )
#define map_try_get( M, K, V ) map_try_get( (M), MAP_KEY(K), V )
#define map_put(M, K, V) map_put( (M), MAP_KEY(K), COL_VALUE(V) )
#define map_remove( M, K) map_remove( (M), MAP_KEY(K)  )


// map_create() default arguments
#define map_create_0() (map_create)( 0, MAP_DEFAULT_DATA_POLICIES, MEM_DEFAULT_MALLOC_POLICY)
#define map_create_1(_1) (map_create)(_1, MAP_DEFAULT_DATA_POLICIES, MEM_DEFAULT_MALLOC_POLICY)
#define map_create_2(_1, _2) (map_create)(_1, _2, MEM_DEFAULT_MALLOC_POLICY)
#define map_create_3(_1, _2, _3) (map_create)(_1, _2, _3)
#define map_create_SELECT_(_1, _2, _3, NAME, ...) NAME
#define map_create(...) \
map_create_SELECT_(__VA_ARGS__ __VA_OPT__(,) map_create_3, map_create_2, map_create_1, map_create_0 ) (__VA_ARGS__)


// utility methods
void display_type_sizes();
