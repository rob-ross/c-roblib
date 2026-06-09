//
// Created by Rob Ross on 2/12/26.
//

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/**
 * Returns the character at the specified index as a Unicode code point.
 * @param utf8_str the input string encoded as utf-8
 * @param index the zero-based index of the character to retrieve
 * @return the character at the specified index, or UINT32_MAX if index is out of bounds.
 */
uint32_t sutil8_char_at(const char *utf8_str, size_t index);

/**
 * Returns the count of unicode code points in the utf-8 string. I.e., the logical character count.
 * @param utf8_str the utf-8 encoded string
 * @return the count of characters, i.e.,  unicode code points in the string.
 */
size_t sutil8_char_count(const char *utf8_str);


/**
 * Returns true if the utf-8-encoded char `utf8_char` is found in the utf-8-encoded `utf8_chars` array.
 * @param utf8_char the character to check, encoded as utf-8
 * @param utf8_chars the characters to check against, encoded as utf-8
 * @return true if char `utf8_char` is found in `utf8_chars`, otherwise return false.
 */
bool sutil8_char_in(uint32_t utf8_char, const char *utf8_chars);