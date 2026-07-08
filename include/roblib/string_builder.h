//  string_builder.h
// Created by Rob Ross on 6/9/26.
//

#pragma once
#ifndef C_ROBLIB_STRING_BUILDER_H
#define C_ROBLIB_STRING_BUILDER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A mutable sequence of characters.
 */
typedef struct string_builder_s {
    uint32_t  capacity;
    uint32_t  length;
    char   *buffer;
} StringBuilder;


/**
 * Initializes the StringBuilder in the argument with the capacity supplied. If `str` is non-null, it will be copied
 * to the StringBuilder's internal buffer. If `capacity` is less than the size of `str`, `capacity` will be adjusted
 * upwards to be at least as long as `str`.
 * @param sb
 * @param capacity
 * @param str
 * @return nullptr if an error occurs, or the argument StringBuilder if successful
 */
StringBuilder * sb_init( StringBuilder *sb, uint32_t capacity, char const * str);

/**
 * Frees memory associated with the StringBuilder
 * @param sb The StringBuilder to destroy
 */
void sb_destroy( StringBuilder *sb);


StringBuilder * sb_append_char( StringBuilder *sb, uint8_t c);
StringBuilder * sb_append_str( StringBuilder *sb, char const *str);

/**
 * Returns the current capacity.
 * @param sb The StringBuilder for which to return the capacity
 * @return Returns the current capacity.
 */
uint32_t sb_capacity( StringBuilder *sb );

/**
 * Copies the characters in a StringBuilder's buffer to the `out_buffer`.
 * @param sb
 * @param out_buffer
 */
void sb_copy_to(StringBuilder *sb, uint32_t buf_len, char out_buffer[static buf_len + 1]);
StringBuilder * sb_insert_char( StringBuilder *sb, uint8_t c, uint32_t index);
StringBuilder * sb_insert_str( StringBuilder *sb, char const * str, uint32_t index);

/**
 *  Returns the length (character count).
 */
uint32_t sb_length( StringBuilder *sb );


void sb_repr( StringBuilder *sb );

#ifdef __cplusplus
}
#endif



#endif //C_ROBLIB_STRING_BUILDER_H
