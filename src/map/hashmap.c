// hashmap.c
//
// Copyright (c) Rob Ross 2026.
//
//


// DO NOT INCLUDE string_counter.h. There is a cyclic dependency between that header file and hashmap.h.
// the declarations needed by both string_counter.c and hashmap.c are in hashmap_private.h.

#include "hashmap.h"
#include "hashmap_private.h"
#include "../memory/memory_pool.h"

#include <string.h> // Required for memcpy
// #include <math.h>
#include <stdlib.h>
#include <stdio.h>






const MapKey   NULL_MAP_KEY   = (MapKey){  .kvoid_ptr = nullptr, .key_type   = COL_TYPE_NULL};
const MapNode  NULL_MAP_NODE  = (MapNode){ .key = NULL_MAP_KEY, .value = NULL_COL_VALUE, .hash = 0, .next = nullptr};



// -----------------------------------------------------------------
//      string_counter.h API methods
//
// We can't include string_counter.h due to circular dependencies
// -----------------------------------------------------------------
StringCounter * (sct_create)(size_t num_buckets, MapDataPolicies data_policies, MemPolicy mem_policy);
extern const MapValuePolicy MAP_STRING_POOL_VALUE_POLICIES;
extern const MapDataPolicies SCT_DEFAULT_DATA_POLICIES;  // need for map_create_using_stringpool()


// -----------------------------------------------------------------
//      Forward References
//
// for functions defined in this file
// -----------------------------------------------------------------

static void map_free_key_if(HashMap map[static 1], const MapNode node[static 1]);
static void map_free_value_if(HashMap map[static 1], const MapNode *node);
static size_t map_hash_mix64(size_t x);
static ColValue map_policy_value_set_default(HashMap map[static 1], ColValue value);
static void map_policy_value_free_default(HashMap map[static 1], ColValue value) ;




//// ------------------------------------------------------------
////
////    Static/private/internal methods
////
//// ------------------------------------------------------------


static void map_free_key_if(HashMap map[static 1], const MapNode node[static 1]) {
    if (!map || !node) return;
    if (map->data_policies.key_policy.on_free_key) {
        map->data_policies.key_policy.on_free_key(map, node->key);
    } else {
        // no key free policy, use default
        map_policy_key_free_default(map, node->key);
    }
}

static void map_free_value_if(HashMap map[static 1], const MapNode node[static 1]) {
    if (!map || !node) return;
    if (map->data_policies.value_policy.on_free_value) {
        map->data_policies.value_policy.on_free_value(map, node->value);
    } else {
        map_policy_value_free_default(map, node->value);
    }
}
// hash mixer
static size_t map_hash_mix64( size_t x ) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

//djb2 hash algorithm, O(N) (actually Theta(N))
static size_t map_hash_string(const char *str) {
    unsigned long hash = 5381; // A "magic" prime number
    int c;

    while (  (c = (unsigned char)(*str++) ) ) {
        // hash = (hash * 33) + c
        // This is a fast way to write it using bit shifts:
        hash = ((hash << 5) + hash) + c;
    }

    return (size_t)hash;
}


//// ------------------------------------------------------------
////
////    Default data policy functions
////
//// ------------------------------------------------------------

// ------------------------------
// Key data_policies
// ------------------------------

MapKey map_policy_key_add_default(HashMap map[static 1], MapKey key) {
    // default add always make a copy of a string key and we own it
    if (!map) return NULL_MAP_KEY;
    ColValuePolicyType keypolicy = map->data_policies.key_policy.policy_type;
    if (key.key_type == COL_TYPE_STRING &&  ( keypolicy == COL_VALUE_POLICY_COPY || keypolicy == COL_VALUE_POLICY_NONE )) {
        char *string_copy = mem_strdup(map->mem_policy, key.kstring);
        return (MapKey){.key_type = COL_TYPE_STRING, .kstring = string_copy};
    }
    return key;
}

void map_policy_key_free_default(HashMap map[static 1], MapKey key) {
    if (!map) return;
    ColValuePolicyType keypolicy = map->data_policies.key_policy.policy_type;
    if (key.key_type == COL_TYPE_STRING &&
        ( keypolicy == COL_VALUE_POLICY_COPY || keypolicy == COL_VALUE_POLICY_TAKE || keypolicy == COL_VALUE_POLICY_NONE )) {
        mem_free_bytes(map->mem_policy, key.kstring);  //we own it.
    }
}

// -----------------------
// Value data_policies
// -----------------------

static ColValue map_policy_value_set_default(HashMap map[static 1], ColValue value) {
    // default add always make a copy of a string key and we own it
    if (!map) return NULL_COL_VALUE;
    ColValue result = col_policy_value_set_default(map, value, map->data_policies.value_policy.policy_type, map->mem_policy);
    return result;
}

static void map_policy_value_free_default(HashMap map[static 1], ColValue value) {
    if (!map) return;
    col_policy_value_free_default(map, value, map->data_policies.value_policy.policy_type, map->mem_policy);
}

const MapKeyPolicy   MAP_DEFAULT_KEY_POLICY = (MapKeyPolicy){
    .context         = nullptr,
    .policy_type     = COL_VALUE_POLICY_COPY,
    .on_add_key      = map_policy_key_add_default,
    .on_free_key     = map_policy_key_free_default,
    .on_free_context = nullptr,
};

const MapValuePolicy MAP_DEFAULT_VALUE_POLICY = (MapValuePolicy){
    .context         = nullptr,
    .policy_type     = COL_VALUE_POLICY_COPY,
    .on_set_value    = map_policy_value_set_default,
    .on_free_value   = map_policy_value_free_default,
    .on_free_context = nullptr,
};

const MapDataPolicies MAP_DEFAULT_DATA_POLICIES = (MapDataPolicies){
    .key_policy   = MAP_DEFAULT_KEY_POLICY,
    .value_policy = MAP_DEFAULT_VALUE_POLICY
};

//// ------------------------------
//// End default data policy functions
//// ------------------------------




//// ------------------------------------------------------------
////
////    'Package-private' / 'friend' API methods
////
////    declared in hashmap_private.h
//// ------------------------------------------------------------

size_t map_calc_bucket_index(const size_t hashcode, const size_t num_buckets) {
    return hashcode & (num_buckets - 1);  // works because num_buckets is a power of 2
}

MapNode * map_create_node(HashMap map[static 1], const size_t hashcode, const MapKey key) {

    MapNode *new_node = (MapNode *)mem_alloc_bytes(map->mem_policy, sizeof(MapNode));
    if (new_node == nullptr) {
        // Handle allocation failure (in a real app, maybe return status)
        return nullptr;
    }

    // Resolve the key to be stored (handling string copying via policy if needed).
    MapKey stored_key = key;
    if (key.key_type == COL_TYPE_STRING) {
        if (map->data_policies.key_policy.on_add_key) {
            stored_key = map->data_policies.key_policy.on_add_key(map, key);
        } else {
            stored_key = map_policy_key_add_default(map, key);
        }
    }

    // Initialize a temporary node on the stack.
    // This allows us to set the 'const' members using standard initialization.
    MapNode temp = {
        .hash = hashcode,
        .key = stored_key,
        .value = {},
        .next = nullptr
    };

    // Copy the initialized content into the allocated memory.
    // This safely sets the const members before the memory is exposed as a MapNode.
    memcpy(new_node, &temp, sizeof(MapNode));

    return new_node;
}

void map_destroy_node(HashMap map[static 1], MapNode node[static 1]) {
    map_free_key_if(map, node);
    map_free_value_if(map, node);
    mem_free_bytes(map->mem_policy, node);
}

void map_ensure_capacity(HashMap map[static 1], const unsigned wanted_capacity) {
    // printf("map_ensure_capacity: buckets: %'zu->%'zu\n", map->num_buckets, map->num_buckets * 2);

    if (wanted_capacity < map->fill_capacity) {
        // current capacity meets need
        return;
    }
    const size_t wanted_buckets = (size_t)(wanted_capacity / map->fill_factor);
    size_t new_num_buckets;

    while  ( (new_num_buckets = map->num_buckets * 2) < wanted_buckets && new_num_buckets < MAX_NUM_BUCKETS ) {
        //num_buckets always a power of 2
        // NO-OP
    }

    if (new_num_buckets > MAX_NUM_BUCKETS) {
        new_num_buckets = MAX_NUM_BUCKETS;
    }

    if ( new_num_buckets <= map->num_buckets) {
        // map already at max bucket size
        return;
    }

    // todo we should call our mem_realloc_bytes method here
    MapNode **new_buckets = \
        (MapNode **)mem_calloc_bytes(map->mem_policy, new_num_buckets,  sizeof(MapNode *));
    if (!new_buckets) {
        return; // Allocation failed, keep old map
    }

    // Rehash all existing nodes
    for (size_t i = 0; i < map->num_buckets; i++) {
        MapNode *current = map->buckets[i];
        while (current != nullptr) {
            MapNode *next = current->next; // Save next pointer

            // Calculate new index
            size_t new_index = map_calc_bucket_index(current->hash, new_num_buckets);

            // Insert into new bucket (at head)
            current->next = new_buckets[new_index];
            new_buckets[new_index] = current;

            current = next;
        }
    }

    mem_free_bytes(map->mem_policy, map->buckets);
    map->buckets = new_buckets;
    map->num_buckets = new_num_buckets;
    map->fill_capacity = (size_t)(new_num_buckets * (long double)map->fill_factor);
    map_recalc_load(map);
}

size_t map_hash_function(const MapKey key) {
    size_t raw_hash;
    switch (key.key_type) {
        case COL_TYPE_LONG: {
            // For integer keys, especially sequential ones, we multiply by a large
            // prime to spread the bits out across the 64-bit range. This ensures that
            // the mixer step works effectively. 0x9e3779b97f4a7c15 is a
            // common choice related to the golden ratio.
            raw_hash =  key.klong * 0x9e3779b97f4a7c15ULL;
            break;
        }
        case COL_TYPE_DOUBLE: {
            double d = key.kdouble;
            // Normalize -0.0 to 0.0 so they hash to the same bucket
            if (d == 0.0) d = 0.0;
            size_t hash = 0;
            if (sizeof(size_t) >= sizeof(double)) {
                memcpy(&hash, &d, sizeof(double));
            } else {
                // ReSharper disable  CppDFAUnreachableCode
                unsigned long long bits;
                memcpy(&bits, &d, sizeof(double));
                hash = (size_t)(bits ^ (bits >> 32));
            }
            raw_hash = hash;
            break;
        }
        case COL_TYPE_STRING: {
            raw_hash = map_hash_string((char*)key.kstring);
            break;
        }
        case COL_TYPE_VOID_PTR: {
            // this requires the caller to have defined a hash function for this blob.
            raw_hash = (unsigned long)key.kvoid_ptr;
            break;
        }
        case COL_TYPE_NONE:
        default:
            raw_hash = 0;
    }
    return map_hash_mix64(raw_hash);
}

bool map_equals_MapKeyPolicy(const MapKeyPolicy kp1, const MapKeyPolicy kp2) {
    if (kp1.context         != kp2.context ||
        kp1.on_add_key      != kp2.on_add_key ||
        kp1.on_free_key     != kp2.on_free_key ||
        kp1.on_free_context != kp2.on_free_context) {
        return false;
    }
    return true;
}

bool map_equals_MapValuePolicy(const MapValuePolicy vp1, const MapValuePolicy vp2) {
    if (vp1.context         != vp2.context ||
        vp1.on_set_value    != vp2.on_set_value ||
        vp1.on_free_value   != vp2.on_free_value ||
        vp1.on_free_context != vp2.on_free_context) {
        return false;
    }
    return true;
}

bool map_equals_MapDataPolicies(const MapDataPolicies o1, const MapDataPolicies o2) {
    const MapKeyPolicy kp1 = o1.key_policy;
    const MapKeyPolicy kp2 = o2.key_policy;
    const MapValuePolicy vp1 = o1.value_policy;
    const MapValuePolicy vp2 = o2.value_policy;

    return map_equals_MapKeyPolicy(kp1, kp2) && map_equals_MapValuePolicy(vp1, vp2);
}

bool map_equals_MapKey(const MapKey k1, const MapKey k2) {

    if (k1.key_type != k2.key_type) return false;

    switch (k1.key_type) {
        case COL_TYPE_NONE:
            return false;
        case COL_TYPE_LONG:
            return k1.klong == k2.klong;
        case COL_TYPE_DOUBLE:
            return col_equals_double(k1.kdouble, k2.kdouble);
        case COL_TYPE_STRING: {
            if (k1.kstring == k2.kstring) return true;
            return strcmp(k1.kstring, k2.kstring) == 0;
        }
        case COL_TYPE_VOID_PTR:
            // this requires the caller to have defined an equal function for this blob.
            // todo implement
            return k1.kvoid_ptr == k2.kvoid_ptr;
        case COL_TYPE_NULL:
            return true; // both key_types are null
        default:
            return false;
    }
}

MapNode * map_node_for(const HashMap map[static 1], const MapKey key) {
    if (map == nullptr) return nullptr;
    const size_t index = map_calc_bucket_index(map_hash_function(key), map->num_buckets);

    MapNode *current = map->buckets[index];

    while (current != nullptr) {
        if ( map_equals_MapKey(key, current->key) ) {
            return current;
        }
        current = current->next;
    }
    return nullptr; // Key not found
}

void map_set_value(HashMap map[static 1], MapNode node[static 1], const ColValue value ) {
    if (value.value_type == COL_TYPE_STRING) {
        if ( map->data_policies.value_policy.on_set_value ) {
            node->value.vstring = map->data_policies.value_policy.on_set_value(map, value).vstring;
        } else {
            node->value.vstring = map_policy_value_set_default(map, value).vstring;
        }
        node->value.value_type = value.value_type;
    } else {
        node->value = value;
    }
}

void map_recalc_load(HashMap *map) {
    map->load =  (double) ((long double)map->size / map->num_buckets);
}


//// ------------------------------------------------------------
////    End 'Package-private' / 'friend' API methods
//// ------------------------------------------------------------





//// ------------------------------------------------------------
////
////    Public API methods
////
//// ------------------------------------------------------------


// returns nullptr on failure. if initial_capacity == 0, uses 16 as initial bucket size.
// number of buckets doubles when fill capacity is reached (75% full by default).
// 16 buckets provides adequate sizing for 12 items before growing HashMap capacity
// initial_capacity is number of elements that can be stored in the HashMap before resizing.
HashMap * (map_create)(unsigned initial_capacity, MapDataPolicies data_policies, MemPolicy mem_policy) {
    // ReSharper disable  CppPrintfExtraArg
    // ReSharper disable  CppPrintfBadFormat
    // printf("\nmap_create: initial_capacity was : %'u, ", initial_capacity);
    if (initial_capacity < MIN_CAPACITY) {
        initial_capacity = MIN_CAPACITY;
    } else if ( initial_capacity > MAX_CAPACITY ) {
        initial_capacity = MAX_CAPACITY;
    }
    size_t num_buckets = (size_t)(initial_capacity /  DEFAULT_FILL_FACTOR);

    num_buckets = col_next_power_of_two(num_buckets, MIN_NUM_BUCKETS, MAX_NUM_BUCKETS);

    // printf("initial_capacity now : %'u, num_buckets = %'zu\n", initial_capacity, num_buckets);


    HashMap *map = (HashMap *)mem_alloc_bytes(mem_policy, sizeof(HashMap));

    if (map == nullptr) {
        return nullptr;
    }


    MapNode **buckets = (MapNode **)mem_calloc_bytes(mem_policy, num_buckets, sizeof(MapNode *));
    if (!buckets) {
        mem_free_bytes(mem_policy, map);
        return nullptr;
    }

    HashMap prototype = (HashMap){
        .buckets = buckets,
        .size = 0,
        .fill_capacity = (size_t)( num_buckets * (long double)DEFAULT_FILL_FACTOR ),
        .load = 0,
        .num_buckets = num_buckets,
        .fill_factor = DEFAULT_FILL_FACTOR,
        .data_policies= data_policies,
        .mem_policy  = mem_policy,
        .flags = 0 };

    memcpy(map, &prototype, sizeof(HashMap));
    // printf("returnin from map_create: "); map_repr_HashMap(map, false, ""); printf("\n");

    return map;
}

//todo fix this. pass mem_policy by value?
// convenience method that creates a HashMap backed by a StringCounter to use as a string pool for sharing string
// instances, if there are many duplicate string values. MemPolicy can be nullptr, or a valid allocator.
HashMap *map_create_using_stringpool(size_t num_buckets, const MemPolicy *mem_policy) {
    if ( !mem_policy ) mem_policy = &MEM_DEFAULT_MALLOC_POLICY;

    HashMap *map = map_create(num_buckets, MAP_DEFAULT_DATA_POLICIES, *mem_policy);
    if (!map) return nullptr;

    // The StringCounter is subordinate to the HashMap in `map`.
    // The StringCounter must not free the memory pool when destroyed. This is the HashMap's responsibility.
    MemPolicy mp = *mem_policy;
    if (mp.policy_type == MEM_POLICY_ALLOCATOR_OWN) {
        mp.policy_type = MEM_POLICY_ALLOCATOR_SHARED;
    } else if (mp.policy_type == MEM_POLICY_MALLOC_OWN ) {
        mp.policy_type = MEM_POLICY_MALLOC_SHARED;
    }

    StringCounter *sct = sct_create(num_buckets , SCT_DEFAULT_DATA_POLICIES, mp);
    if (!sct) {
        map_destroy(map);
        return nullptr;
    }


    map->data_policies.key_policy = MAP_DEFAULT_KEY_POLICY;
    map->data_policies.value_policy = MAP_STRING_POOL_VALUE_POLICIES;

    //add the string pool map as the context for the enclosing HashMap's value policy
    map->data_policies.value_policy.context = sct;


    return map;
}


void map_destroy(HashMap map[static 1]) {
    if (!map) return;
    MemPolicy mem_policy = map->mem_policy;
    if (mem_policy.free_context && mem_policy.policy_type == MEM_POLICY_ALLOCATOR_OWN) {
        mem_policy.free_context(mem_policy.context);
    } else {
        map_clear(map);

        // free any data policy contexts that exist
        if (map->data_policies.key_policy.on_free_context) {
            map->data_policies.key_policy.on_free_context(map->data_policies.key_policy.context);
        }
        map->data_policies.key_policy.context = nullptr;

        if (map->data_policies.value_policy.on_free_context) {
            map->data_policies.value_policy.on_free_context(map->data_policies.value_policy.context);
        }
        map->data_policies.value_policy.context = nullptr;

        mem_free_bytes(mem_policy, map->buckets);
        map->buckets = nullptr;

        mem_free_bytes(mem_policy, map);

        if (mem_policy.policy_type == MEM_POLICY_MALLOC_OWN) {
            free(mem_policy.context);
        }
    }
}

// deletes and frees all Nodes but does not reduce bucket size or free allocated bucket memory.
// todo some kind of resize method to realloc to a smaller memory footprint?
void map_clear(HashMap map[static 1]) {
    const size_t num_buckets = map->num_buckets;
    for (size_t i = 0; i < num_buckets; i++) {
        MapNode *current = map->buckets[i];
        while (current != nullptr) {
            MapNode *temp = current;
            current = current->next;
            map_destroy_node(map, temp);
        }
        map->buckets[i] =  nullptr;
    }
    map->load = 0;
    map->size = 0;
}

bool (map_contains_key)(HashMap map[static 1], const MapKey key) {
    ColValue unused;
    return (map_try_get)(map, key, &unused);
}


// initial implementation is O(N)
bool (map_contains_value)(HashMap map[static 1], const ColValue value) {
    const size_t num_buckets = map->num_buckets;
    for ( size_t index = 0; index < num_buckets; ++index ) {
        const MapNode *current = map->buckets[index];
        while (current) {
            if ( col_equals_ColValue(value, current->value) ) {
                return true;
            }
            current = current->next;
        }
    }
    return false;
}

ColValue (map_get)(const HashMap map[static 1], const MapKey key) {
    if (map == nullptr) return NULL_COL_VALUE;
    const size_t index = map_calc_bucket_index(map_hash_function(key), map->num_buckets);

    MapNode const *current = map->buckets[index];

    while (current != nullptr) {
        if ( map_equals_MapKey(key, current->key) ) {
            return current->value;
        }
        current = current->next;
    }
    return NULL_COL_VALUE; // Key not found
}

ColValue (map_get_or)(const HashMap map[static 1], const MapKey key, const ColValue fallback) {
    const ColValue value = (map_get)(map, key);
    if (value.value_type == COL_TYPE_NULL) {
        return fallback;
    }
    return value;
}

bool (map_try_get)(const HashMap map[static 1], const MapKey key, ColValue *out) {
    const ColValue value = (map_get)(map, key);
    if (value.value_type == COL_TYPE_NULL) {
        *out = NULL_COL_VALUE;
        return false;
    }
    *out = value;
    return true;
}

bool map_is_empty(const HashMap map[static 1]) {
    return map->size == 0;
}

// if key or value are strings, they are copied so the map can be free them independently of the original arguments.
void (map_put)(HashMap map[static 1], const MapKey key, const ColValue value) {
    if (map == nullptr) return;

    if (map->size >= map->fill_capacity) {
        map_ensure_capacity(map, map->size + 1);
    }

    const size_t hashcode = map_hash_function(key);
    const size_t bucket_index = map_calc_bucket_index(hashcode, map->num_buckets);
    MapNode *current = map->buckets[bucket_index];

    // Check if key already exists and update value
    while (current != nullptr) {
        if ( map_equals_MapKey(key, current->key) ) {
            map_free_value_if(map, current);
            map_set_value(map, current, value);
            return;
        }
        current = current->next;
    }
    // Key not found, insert new node at the beginning of the list
    MapNode *new_node = map_create_node(map, hashcode, key);
    if (new_node == nullptr) {
        // Handle allocation failure (in a real app, maybe return status)
        return;
    }
    new_node->next =  map->buckets[bucket_index];  // inserts this MapNode at the head of the bucket
    map_set_value(map, new_node, value);
    map->buckets[bucket_index] = new_node;
    map->size++;
    map_recalc_load(map);

}

void (map_remove)(HashMap map[static 1], const MapKey key) {
    if (!map) return;

    const size_t hashcode = map_hash_function(key);
    const size_t index = map_calc_bucket_index(hashcode, map->num_buckets);

    MapNode *current = map->buckets[index];
    MapNode *prev = nullptr;

    while (current) {
        if ( map_equals_MapKey(key, current->key ) ){
            if (!prev) {
                map->buckets[index] = current->next;
            } else {
                prev->next = current->next;
            }
            map_destroy_node(map, current);
            map->size--;
            map_recalc_load(map);
            return;
        }
        prev = current;
        current = current->next;
    }
}

size_t map_size(const HashMap map[static 1]) {
    return map->size;
}



//// ---------------------------------------------
////  repr methods
//// ---------------------------------------------

void map_repr_HashMap(const HashMap map[static 1], const bool verbose, const char* type_str) {
    if (!type_str || type_str[0] == '\0') type_str = "HashMap";
    if (!map) {
        printf("(%s)nullptr",type_str);
        return;
    }

    // ReSharper disable CppPrintfBadFormat
    // ReSharper disable CppPrintfExtraArg
    printf( "(%s){ .size=%'zu, .fill_capacity=%'zu, .load=%'g, "
            ".num_buckets=%'zu, .fill_factor=%g, .mem_policy_type = %hhd"
            "}", type_str,
            map->size, map->fill_capacity, map->load,
            map->num_buckets, map->fill_factor, map->mem_policy.policy_type);


    // we might want a special Bucket struct to be the head of each bucket, so we can keep statistics on the bucket
    // contents
    // For now we'll do it the hard way and generate stats on the fly by visiting every MapNode in the HashMap
    printf("\n");
    if (verbose) {
        constexpr int max_buckets_displayed = 16;
        size_t total_bucket_items = 0;
        printf("bucket sizes: ");
        for (size_t i=0; i < map->num_buckets; ++i) {
            const MapNode *current = map->buckets[i];
            size_t node_count = 0;
            while (current) {
                node_count++;
                current = current->next;
            }
            total_bucket_items += node_count;
            if ( i < max_buckets_displayed) {
                printf("[%zu]=%'zu, ", i, node_count);
            }
        }
        if ( map->num_buckets > max_buckets_displayed) {
            printf(" ...");
        }
        printf("\n");
        printf("total bucket items: %'zu\n\n", total_bucket_items);


        // for each bucket, display name/value pairs.
        for (size_t i=0; i < max_buckets_displayed && i < map->num_buckets; ++i) {
            const MapNode *current = map->buckets[i];
            printf("bucket[%zu]:",i);
            if (!current) {
                printf(" empty\n");
            } else {
                printf("\n");
            }
            size_t node_count = 0;
            while ( current) {
                node_count++;
                if ( node_count < max_buckets_displayed) {
                    map_repr_Node(current);
                }
                current = current->next;
            }
            printf("---------------------------------------------------------------------------------------------------------------------\n");
            printf("bucket[%zu] node count:%zu\n",i,node_count);
            printf("\n");
        }
    }

}

void map_repr_MapKey(const MapKey map_key, bool verbose) {

    if (map_key.key_type == COL_TYPE_NONE) {
        printf("(MapKey){ COL_TYPE_NONE }");
        return;
    }
    if (map_key.key_type == COL_TYPE_NULL) {
        printf("(MapKey){ COL_TYPE_NULL }");
        return;
    }
    if (verbose) {
        printf("(MapKey){ ");
    }
    switch (map_key.key_type) {
        case COL_TYPE_LONG:
            printf(".klong=%5lu", map_key.klong);
            break;
        case COL_TYPE_DOUBLE:
            printf(".kdouble=%5g", map_key.kdouble);
            break;
        case COL_TYPE_STRING:
            printf(".kstring='%5s'", map_key.kstring);
            break;
        case COL_TYPE_VOID_PTR:
            printf(".kvoid_ptr=%14p", map_key.kvoid_ptr);
            break;
        default:
            if (verbose) {
                printf("unknown");
            } else {
                printf("(MapKey){ unknown }");
            }
    }
    if (verbose) {
        printf(" }");
    }
}

void map_repr_MapValue(const ColValue map_value, const bool verbose) {
    if (map_value.value_type == COL_TYPE_NONE) {
        printf("(ColValue){ COL_TYPE_NONE }");
        return;
    }
    if (map_value.value_type == COL_TYPE_NULL) {
        printf("(ColValue){ COL_TYPE_NULL }");
        return;
    }
    if (verbose) {
        printf("(ColValue){ ");
    }
    switch (map_value.value_type) {
        case COL_TYPE_LONG:
            printf(".vlong=%5lu", map_value.vlong);
            break;
        case COL_TYPE_DOUBLE:
            printf(".vdouble=%5g", map_value.vdouble);
            break;
        case COL_TYPE_STRING:
            printf(".vstring='%5s'", map_value.vstring);
            break;
        case COL_TYPE_VOID_PTR:
            printf(".vvoid_ptr=%14p", map_value.vvoid_ptr);
            break;
        default:
            if (verbose) {
                printf("unknown");
            } else {
                printf("(ColValue){ unknown }");
            }
    }
    if (verbose) {
        printf(" }");
    }
}

void map_repr_Node(const MapNode node[static 1]) {
    if (!node) {
        printf("(MapNode)nullptr");
        return;
    }
    printf("(MapNode){ ");
    printf(".hash=0x%-16lX", node->hash);
    printf(", .this=%14p, .next=%14p, ", node, node->next);
    map_repr_MapKey(node->key, false);
    printf(", ");
    map_repr_MapValue(node->value, false);
    printf(" }\n");
}





//// -----------------------------------------------------
////    Converters for generic map function arguments
////
////    these convert expressions to a MapKey or ColValue
//// -----------------------------------------------------

MapKey key_for_long(const long k) {
    return (MapKey){.klong = k, .key_type = COL_TYPE_LONG};
}

MapKey key_for_double(const double k) {
    return (MapKey){.kdouble = k, .key_type = COL_TYPE_DOUBLE};
}

MapKey key_for_string(const char * k) {
    return (MapKey){.kstring = (char*)k, .key_type = COL_TYPE_STRING};
}

MapKey key_for_void_ptr(const void * k) {
    return (MapKey){.kvoid_ptr = (void*)k, .key_type = COL_TYPE_VOID_PTR};
}


#include "../base.h"
// utility methods
void display_type_sizes() {
    print("\nhashmap.h data types");
    print("------------------------");
    PVL(sizeof(ColTypeEnum));
    PVL(sizeof(MapKey));
    PVL(sizeof(ColValue));
    PVL(sizeof(MapNode));
    PVL(sizeof(ColValuePolicyType));

    PVL(sizeof(MapKeyPolicy));

    PVL(sizeof(MapValuePolicy));
    PVL(sizeof(MapDataPolicies));
    PVL(sizeof(MemPolicy));
    PVL(sizeof(HashMap));
}