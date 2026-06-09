//
// Created by Rob Ross on 2/12/26.
//
//
// utility functions for C char strings encoded as UTF-8.


#include <stdio.h>
#include <stdint.h>

#include "string_utils_utf8.h"

// Returns the Unicode code point at the given logical character index.
// Returns UINT32_MAX if index is out of bounds or string is invalid.
uint32_t sutil8_char_at(const char *utf8_str, const size_t index) {
    if (!utf8_str) return UINT32_MAX;

    size_t current_index = 0;
    const unsigned char *p = (const unsigned char *)utf8_str;

    while (*p) {
        uint32_t code_point = 0;
        int bytes = 0;

        // 1. Determine sequence length based on the first byte
        if ((*p & 0x80) == 0) {           // 1 byte: 0xxxxxxx (ASCII)
            code_point = *p;
            bytes = 1;
        } else if ((*p & 0xE0) == 0xC0) { // 2 bytes: 110xxxxx
            code_point = *p & 0x1F;
            bytes = 2;
        } else if ((*p & 0xF0) == 0xE0) { // 3 bytes: 1110xxxx
            code_point = *p & 0x0F;
            bytes = 3;
        } else if ((*p & 0xF8) == 0xF0) { // 4 bytes: 11110xxx
            code_point = *p & 0x07;
            bytes = 4;
        } else {
            return UINT32_MAX; // Invalid UTF-8 start byte
        }

        // 2. If this is the target index, decode the rest and return
        if (current_index == index) {
            for (int i = 1; i < bytes; i++) {
                p++;
                if ((*p & 0xC0) != 0x80) return UINT32_MAX; // Invalid continuation byte
                code_point = (code_point << 6) | (*p & 0x3F);
            }
            return code_point;
        }

        // 3. Otherwise, skip this character's bytes
        p += bytes;
        current_index++;
    }

    return UINT32_MAX; // Index out of bounds
}

size_t sutil8_char_count(const char *utf8_str) {
    if (!utf8_str) return 0;

    size_t count = 0;
    const unsigned char *p = (const unsigned char *)utf8_str;

    while (*p) {
        // Increment count if this byte is NOT a continuation byte (10xxxxxx)
        if ((*p & 0xC0) != 0x80) count++;
        p++;
    }
    return count;
}