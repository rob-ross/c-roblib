//
// Created by Rob Ross on 2/12/26.
//
//
// utility functions for C char strings encoded as UTF-8.

#include "roblib/string_utils_utf8.h"

#include <stdio.h>
#include <stdint.h>


// Returns the Unicode code point at the given logical character index.
// Returns UINT32_MAX if `index` is out of bounds or if `string` is invalid.
uint32_t sutil8_char_at(const char *utf8_str, const size_t index) {
    if (!utf8_str) return UINT32_MAX;

    size_t current_index = 0;
    const unsigned char *p = (const unsigned char *)utf8_str;

    while (*p) {
        uint32_t code_point = 0;
        int bytes = 0;

        // 1. Determine sequence length based on the first byte
        const signed char b = (signed char)*p;
        if (b >= 0) {                    // 1 byte: 0xxxxxxx (ASCII)
            code_point = (uint32_t)b;
            bytes = 1;
        } else if (b < -64) {            // 10xxxxxx: Invalid lead (Continuation byte)
            return UINT32_MAX;
        } else if (b < -32) {            // ((b & 0xE0) == 0xC0) -> 2 bytes: 110xxxxx
            code_point = b & 0x1F;
            bytes = 2;
        } else if (b < -16) {            // ((b & 0xF0) == 0xE0) -> 3 bytes: 1110xxxx
            code_point = b & 0x0F;
            bytes = 3;
        } else if (b < -8) {             // ((b & 0xF8) == 0xF0) -> 4 bytes: 11110xxx
            code_point = b & 0x07;
            bytes = 4;
        } else {
            return UINT32_MAX; // Invalid UTF-8 start byte
        }

        // 2. If this is the target index, decode the rest and return
        if (current_index == index) {
            for (int i = 1; i < bytes; i++) {
                p++;
                const signed char cb = (signed char)*p;
                // if ((*p & 0xC0) != 0x80) return UINT32_MAX;
                if (cb >= -64) return UINT32_MAX; // Invalid continuation byte (must be < -64)
                code_point = (code_point << 6) | (cb & 0x3F);
            }
            return code_point;
        }

        // 3. Otherwise, move to the next character.
        // We iterate rather than jumping (p += bytes) to ensure we don't
        // over-read a malformed string that ends prematurely.
        while (bytes-- > 0) {
            if (*p == '\0') return UINT32_MAX;
            p++;
        }

        current_index++;
    }

    return UINT32_MAX; // Index out of bounds
}

size_t sutil8_char_count(const char *utf8_str) {
    if (!utf8_str) return 0;

    size_t count = 0;
    const unsigned char *p = (const unsigned char *)utf8_str;

    while (*p) {
        /*
         * Branchless counting: Increment if this byte is NOT a continuation byte.
         * A continuation byte always has the high bits 10xxxxxx.
         * In two's complement, any byte starting with 10xxxxxx is between
         * 0x80 and 0xBF. By casting to a signed char, these are all values
         * less than -64.
         * Therefore: (signed char)b > -65 is true for all start bytes.
         */

        //count += ((*p & 0xC0) != 0x80);
        count += ((signed char)*p > -65);
        p++;
    }
    return count;
}
