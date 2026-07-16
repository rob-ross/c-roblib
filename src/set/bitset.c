// bitset.c
//
// Created by Rob Ross on 6/11/26.
//

#include <stdio.h>

#include "roblib/bitset.h"


bool bitset_test_all(const void *set_ptr, const size_t count, const uint32_t flags[]) {
    const BitSet *set = (const BitSet *)set_ptr;
    for (size_t i = 0; i < count; ++i) {
        uint32_t flag = flags[i];
        if (!(set->words[flag / BITS_PER_WORD] & ((uint64_t)1 << (flag % BITS_PER_WORD)))) {
            return false; // Fail fast
        }
    }
    return true;
}

bool bitset_test_any(const void *set_ptr, const size_t count, const uint32_t flags[]) {
    const BitSet *set = (const BitSet *)set_ptr;
    for (size_t i = 0; i < count; ++i) {
        if (!bitset_test(set, flags[i])) {
            return true;
        }
    }
    return false;
}

bool bitset_contains_mask(const void *set_ptr, const void *mask_ptr) {
    const BitSet *set = (const BitSet *)set_ptr;
    const BitSet *mask = (const BitSet *)mask_ptr;
    const size_t count = set->word_count;
    // Use the count stored in the struct itself!
    for (size_t i = 0; i < count; ++i) {
        if ( ( set->words[i] & mask->words[i] ) != mask->words[i] ) {
            return false;
        }
    }
    return true;
}

// bulk set multiple flags at once using a mask
void bitset_set_mask(void *set_ptr, const void *mask_ptr) {
    BitSet *set = (BitSet *)set_ptr;
    const size_t count = set->word_count;
    const BitSet *mask = (const BitSet *)mask_ptr;
    for (size_t i = 0; i < count; ++i) {
        set->words[i] |= mask->words[i];
    }
}

// bulk clear multiple flags at once using a mask
void bitset_clear_mask(void *set_ptr, const void *mask_ptr) {
    BitSet *set = (BitSet *)set_ptr;
    const size_t count = set->word_count;
    const BitSet *mask = (const BitSet *)mask_ptr;
    for (size_t i = 0; i < count; ++i) {
        set->words[i] &= ~mask->words[i]; // clears 64 flags per iteration
    }
}

// bulk helper function using the VLA/static count parameter syntax
void bitset_set_multiple(void *set_ptr, const size_t count, const uint32_t flags[]) {
    for (size_t i = 0; i < count; ++i) {
        bitset_set(set_ptr, flags[i]);
    }
}


#ifdef BITSET_MAIN
int main(void) {
    // temp test.
    // We could expand this to hundreds or thousands of flags.
    // each enum constant represents the number of bits to left shift the number 1
    enum SystemFlag  : uint32_t{
        FLAG_FIRST = 0,
        // ... possibly hundreds of entries
        // examples:
        FLAG_NETWORK_READY = 452,
        FLAG_DATABASE_CONNECTED = 1023,
        FLAG_MAX_COUNT = 2000,  // total capacity needed

    };

    // 1. Define the custom type
    BITSET_DEFINE(MyBitSet, FLAG_MAX_COUNT);

    // 2. Initialize it (the macro sets word_count to 1)
    MyBitSet my_flags = BITSET_INIT(FLAG_MAX_COUNT);


    // 3. Use generic functions without worrying about sizes



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
        uint32_t critical_group[] = { FLAG_NETWORK_READY, FLAG_DATABASE_CONNECTED};
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
#endif
