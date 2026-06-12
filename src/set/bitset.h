// bitset.h
//
// Created by Rob Ross on 6/11/26.
//

#pragma once
#ifndef C_ROBLIB_BITSET_H
#define C_ROBLIB_BITSET_H


#include <stdint.h>
// #include <stdbool.h>

// We could expand this to hundreds or thousands of flags.
// each enum constant has to be a power of two, or can
// represent the number of bits to left shift the number 1
enum SystemFlag {
    FLAG_FIRST = 0,
    // ... possibly hundreds of entries
    // examples:
    FLAG_NETWORK_READY = 452,
    FLAG_DATABASE_CONNECTED = 1023,
    FLAG_MAX_COUNT = 2000,  // total capacity needed

};

// constants based on 64-bit chunks
#define BITS_PER_WORD 64
#define BITSET_WORDS (( FLAG_MAX_COUNT + ( BITS_PER_WORD - 1 )) / BITS_PER_WORD )

typedef struct bit_set_t {
    uint64_t words[BITSET_WORDS];
} BitSet;


static inline void bitset_set(BitSet *set, enum SystemFlag flag) {
    set->words[ flag / BITS_PER_WORD ] |=  (uint64_t)1 << (flag % BITS_PER_WORD);
}

static inline void bitset_clear(BitSet *set, enum SystemFlag flag) {
    set->words[flag / BITS_PER_WORD ] &= ~( (uint64_t)1 << (flag % BITS_PER_WORD) );
}

static inline bool bitset_test(const BitSet *set, enum SystemFlag flag) {
    return ( set->words[ flag / BITS_PER_WORD] & ( (uint64_t)1 << ( flag % BITS_PER_WORD) ) ) != 0;
}





#endif //C_ROBLIB_BITSET_H
