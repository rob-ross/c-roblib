//  platform_specific.c
//
//  Created by Rob Ross on 8/1/26.
//
//  Copyright (c) 2026.  All rights reserved.

//


#include <stddef.h>
#include <stdint.h>

#if defined(_MSC_VER)
# include <intrin.h> // Required for MSVC bit intrinsics
#endif

inline size_t round_up_to_power_of_two(size_t x) {
    if (x <= 1) return 1;
    x--; // Handle cases where x is already a power of two

#if defined(__GNUC__) || defined(__clang__)
    // todo TEST
    // GCC/Clang: Count leading zeros directly
    return (size_t)1 << (64 - __builtin_clzll(x));

#elif defined(_MSC_VER)
    // todo TEST
    // MSVC: BitScanReverse finds the index of the highest set bit
    unsigned long index;
    if (_BitScanReverse64(&index, (unsigned __int64)x)) {
        return (size_t)1 << (index + 1);
    }
    return 1;

#else
    // todo TEST
    // Pure Portable C Fallback: Bit-shifting loop (No compiler tricks)
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    return x + 1;
#endif

}

extern inline size_t round_up_to_power_of_two(size_t x);
