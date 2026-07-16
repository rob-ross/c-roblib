// bitset.h
//
// Created by Rob Ross on 6/11/26.
//

#pragma once
#ifndef C_ROBLIB_BITSET_H
#define C_ROBLIB_BITSET_H

#include <stdint.h>

// We could expand this to hundreds or thousands of flags.
// each enum constant has to be a power of two, or can
// represent the number of bits to left shift the number 1


// constants based on 64-bit chunks
// a more concise calculation is x/y+!!(x%y)
#define BITS_PER_WORD 64
#define BITSET_WORDS_FOR_COUNT(n) (((n) + (BITS_PER_WORD - 1)) / BITS_PER_WORD)

/**
 * The base structure used by generic functions.
 * Any struct defined with BITSET_DEFINE can be cast to this.
 */
typedef struct {
    const size_t word_count;
    uint64_t words[];
} BitSet;

/**
 * Macro to define and initialize a custom-sized bitset.
 */
#define BITSET_DEFINE(name, max_bits) \
    typedef struct { \
        size_t word_count; \
        uint64_t words[BITSET_WORDS_FOR_COUNT(max_bits)]; \
    } name

#define BITSET_INIT(max_bits) { .word_count = BITSET_WORDS_FOR_COUNT(max_bits), .words = {0} }


// Core manipulation functions using uint32_t for flag indices to allow any enum

// Inline helpers for single-bit operations
static inline void bitset_set(void *set, uint32_t flag) {
    BitSet *b = (BitSet *)set;
    b->words[flag / BITS_PER_WORD] |= (uint64_t)1 << (flag % BITS_PER_WORD);
}

static inline void bitset_clear(void *set, uint32_t flag) {
    BitSet *b = (BitSet *)set;
    b->words[flag / BITS_PER_WORD] &= ~((uint64_t)1 << (flag % BITS_PER_WORD));
}

static inline bool bitset_test(const void *set, uint32_t flag) {
    const BitSet *b = (const BitSet *)set;
    return (b->words[flag / BITS_PER_WORD] & ((uint64_t)1 << (flag % BITS_PER_WORD))) != 0;
}

// Bulk operations
bool bitset_test_all(const void *set, size_t count, const uint32_t flags[]);
bool bitset_test_any(const void *set, size_t count, const uint32_t flags[]);

bool bitset_contains_mask(const void *set, const void *mask);

// bulk set multiple flags at once using a mask
void bit_set_mask( void *set, const void *mask);
// bulk clear multiple flags at once using a mask
void bitset_clear_mask( void *set, const void *mask);
// bulk helper function using the VLA/static count parameter syntax
void bitset_set_multiple(void *set, size_t count, const uint32_t flags[]);

// The Variadic macro
// Example using the macro to set multiple flags:
// BITSET_SET_MULTIPLE(&my_flags, FLAG_FIRST, FLAG_NETWORK_READY, FLAG_DATABASE_CONNECTED);
#define BITSET_SET_MULTIPLE(set_ptr, ...) \
    do { \
        const uint32_t _temp_flags[] = { __VA_ARGS__ }; \
        bitset_set_multiple((set_ptr), sizeof(_temp_flags)/sizeof(uint32_t), _temp_flags); \
    } while(0)

#endif //C_ROBLIB_BITSET_H
