// bitset.c
//
// Created by Rob Ross on 6/11/26.
//

#include <stdio.h>

#include "bitset.h"


bool bitset_test_all(const BitSet *set, const size_t flags_len, const enum SystemFlag flags[static flags_len] ) {
    for (int i = 0; i < flags_len; ++i) {
        if (!bitset_test(set, flags[i])) {
            return false; // Fail fast
        }
    }
    return true;
}

bool bitset_test_any(const BitSet * set, const size_t flags_len, const enum SystemFlag flags[static flags_len]) {
    for (int i = 0; i < flags_len; ++i) {
        if (!bitset_test(set, flags[i])) {
            return true;
        }
    }
    return false;
}

bool bitset_contains_mask(const BitSet *set, const BitSet *mask) {
    for (int i = 0; i < BITSET_WORDS; ++i) {
        // if the mask requires on bits that 'set' doesn't have on, it fails
        if ( ( set->words[i] & mask->words[i] ) != mask->words[i] ) {
            return false;
        }
    }
    return true;
}

// bulk set multiple flags at once using a mask
void bit_set_mask(BitSet *set, const BitSet * mask) {
    for (int i = 0; i < BITSET_WORDS; ++i) {
        set->words[i] |= mask->words[i]; // combines 64 flags per iteration
    }
}

// bulk clear multiple flags at once using a mask
void bitset_clear_mask(BitSet * set, const BitSet * mask) {
    for ( size_t i = 0; i < BITSET_WORDS; ++i ) {
        set->words[i] &= ~mask->words[i]; // clears 64 flags per iteration
    }
}


// bulk helper function using the VLA/static count parameter syntax
void bitset_set_multiple(BitSet * set, const size_t count, const enum SystemFlag flags[static count]) {
    for (int i = 0; i < count; ++i) {
        bitset_set(set, flags[i]);
    }
}

// The Variadic macro
#define BITSET_SET_MULTIPLE(set_ptr, ... ) \
    do {                \
        const enum SystemFlag _temp_flags[] = { __VA_ARGS__ }; \
        bitset_set_multiple( (set_ptr), sizeof(_temp_flags) / sizeof (enum SystemFlag), _temp_flags ); \
    } while (0);


int main(void) {
    // temp test
    BitSet my_flags = {};

    // set individual bits out of thousands
    bitset_set(&my_flags, FLAG_NETWORK_READY);
    bitset_set(&my_flags, FLAG_DATABASE_CONNECTED);

    // test individual bits efficiently
    if (bitset_test(&my_flags, FLAG_NETWORK_READY)) {
        printf("Flag 452 is ON\n");
    }

    //clear a bit
    bitset_clear(&my_flags, FLAG_NETWORK_READY);
    if (bitset_test(&my_flags, FLAG_NETWORK_READY)) {
        printf("Flag 452 is OFF\n");
    }

    for (int i = 0; i<2; ++i) {
        enum SystemFlag critical_group[] = { FLAG_NETWORK_READY, FLAG_DATABASE_CONNECTED};
        if ( bitset_test_all(&my_flags, 2, critical_group )) {
            printf("Both flags are set\n");
        } else {
            printf("One or more flags was unset.\n");
        }
        bitset_set(&my_flags, FLAG_NETWORK_READY); // now set it
    }


    // creating a static bit mask at initialization:
    BitSet CRITICAL_SUBSYSTEMS_MASK = {};
    bitset_set(&CRITICAL_SUBSYSTEMS_MASK, FLAG_NETWORK_READY);
    bitset_set(&CRITICAL_SUBSYSTEMS_MASK, FLAG_DATABASE_CONNECTED);

    // Test thousands of bits instantly across all chunks
    if (bitset_contains_mask(&my_flags, &CRITICAL_SUBSYSTEMS_MASK )) {
        printf("All mask bits are set in my_flags.\n");
    } else {
        printf("Not all mask bits are set in my_flags.\n");
    }

    // using the macro to set multiple flags:
    BITSET_SET_MULTIPLE(&my_flags, FLAG_FIRST, FLAG_NETWORK_READY, FLAG_DATABASE_CONNECTED);



    return 0;
}