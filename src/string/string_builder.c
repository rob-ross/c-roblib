//
// Created by Rob Ross on 6/9/26.
//

#include "roblib/string_builder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
 * Todo this code currently uses malloc/calloc when allocating buffers. Add the ability to configure a custom allocator
 * to use for allocations.
 *
 *
 */

// we want our buffer to be around 75% capacity on average to allow quicker appends
constexpr double FILL_RATIO = 3.0 / 4.0 ;
constexpr double CAPACITY_FACTOR = 1.0 / FILL_RATIO;

StringBuilder * sb_init( StringBuilder *sb, uint32_t capacity, char const * str) {
    // todo check for too-large strings? Over 2GB of characters?
    int str_len = strlen(str);
    uint32_t ideal_capacity = (uint32_t)(str_len * CAPACITY_FACTOR);
    if (ideal_capacity == 0) ideal_capacity = 16;
    if ( capacity < ideal_capacity ) capacity = ideal_capacity;

    char * buf = calloc(capacity + 1, sizeof(char));
    if (!buf) {
        return nullptr;
    }
    sb->buffer = buf;
    memcpy(sb->buffer, str, str_len);
    sb->buffer[str_len] = '\0';
    sb->capacity = capacity;
    sb->length = str_len;
    return sb;
}


void sb_destroy(StringBuilder *sb) {
    free(sb->buffer);
    sb->capacity = 0;
    sb->length = 0;
}


static bool sb_ensure_capacity(StringBuilder *sb, uint32_t capacity_wanted) {
    if (capacity_wanted <= sb->capacity) return true;
    // must grow capacity
    uint32_t new_capacity = (uint32_t)(capacity_wanted * CAPACITY_FACTOR);
    char *buf = realloc(sb->buffer, new_capacity);
    if (!buf) return false;
    sb->buffer = buf;
    sb->capacity = new_capacity;
    return true;
}

void sb_copy_to(StringBuilder *sb, uint32_t buf_size, char out_buffer[static buf_size ]) {
    uint32_t copy_len = buf_size;
    if (sb->length < copy_len) {
        copy_len = sb->length;
    }
    strncpy(out_buffer, sb->buffer, copy_len );
    out_buffer[buf_size-1] = '\0';

}

StringBuilder * sb_append_char( StringBuilder *sb, char c) {
    uint32_t new_length = sb->length + sizeof(char);
    if (!sb_ensure_capacity(sb, new_length + 1)) return nullptr;

    if (c == '\0') return sb;  // adding empty string does nothing.

    sb->buffer[sb->length++] = c;
    sb->buffer[sb->length] = '\0';
    sb->length = new_length;

    return sb;
}

StringBuilder * sb_append_str(StringBuilder *sb, char const *str) {
    uint32_t old_len = sb->length;
    uint32_t new_length = sb->length + strlen(str);
    if (!sb_ensure_capacity(sb, new_length + 1)) return nullptr;
    strcpy(sb->buffer + old_len, str );
    sb->length = new_length;
    return sb;
}


void sb_repr( StringBuilder *sb ) {
    printf("(StringBuilder){ .capacity=%d, .length=%d, .buffer=\"%s\" }\n", sb->capacity, sb->length, sb->buffer);
}
