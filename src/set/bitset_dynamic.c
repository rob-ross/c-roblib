// bitset_dynamic.c
//
// Created by Rob Ross on 6/11/26.
//


#include <stdint.h>
#include <stdlib.h>


// -----------------------------------------------------------------
//      Struct-based, type-safe macro (Best for static compilation)
// -----------------------------------------------------------------

// Generic macro to declare a custom-sized bitset structure
#define DECLARE_BITSET_TYPE(TYPE_NAME, MAX_BITS) \
    typedef struct { \
        uint64_t words[ ( (MAX_BITS) + 63 ) /  64 ]; \
    } TYPE_NAME \

// Domain 1: Component Registry (ECS)
typedef enum { COMP_POSITION, COMP_VELOCITY, COMP_RENDER, COMP_AI, COMP_MAX } ComponentType;
DECLARE_BITSET_TYPE( ComponentBitSet, COMP_MAX );  // only takes 8 bytes (one 64-bit word)

// Domain 2: Input/Controls
typedef enum { INPUT_UP, INPUT_DOWN, INPUT_LEFT, INPUT_RIGHT, INPUT_ACTION, INPUT_MAX } InputFlag;
DECLARE_BITSET_TYPE( InputBitSet, INPUT_MAX );  // only takes 8 bytes

// Domain 3: Heavy Game Physics Layers
#define MAX_PHYSICS_LAYERS 150
DECLARE_BITSET_TYPE(PhysicsBitSet, MAX_PHYSICS_LAYERS);  // TAKES 24 bytes (3 64-bit words)


// inside your Entity structure:
typedef struct {
    uint32_t entity_id;
    ComponentBitSet components;   // track which components are attached
    PhysicsBitSet   physics_mask; // tracks collision layers
} Entity;



// -----------------------------------------------------------------
//      Runtime dynamic BitSet (best for fully dynamic composition)
// -----------------------------------------------------------------

typedef struct {
    uint32_t total_bits; // tracks how many bits this specific instance can hold
    uint32_t word_count; // pre-calculated ((total_bits + 63) / 64)
    uint64_t words[];     // dynamically allocated flexible array.
} DynamicBitSet;

// Allocates exactly the memory needed for this specific set
DynamicBitSet * dynamic_bitset_create(uint32_t max_flags) {
    uint32_t word_count = (max_flags + 63) / 64;

    DynamicBitSet * set = (DynamicBitSet *)calloc( 1, sizeof(DynamicBitSet) + sizeof (uint64_t) * word_count);
    if (!set) return nullptr;

    set->total_bits = max_flags;
    set->word_count = word_count;
    return set;
}

// runtime bounds-checked operations:
void dynamic_bitset_set(DynamicBitSet * set, uint32_t flag_index) {
    if (flag_index < set->total_bits) {
        set->words[ flag_index / 64 ] |= ((uint64_t)1 << (flag_index % 64));
    }
}

void dynamic_bitset_destroy(DynamicBitSet * set) {
    free(set);
}