//  string_builder.c
//
// Created by Rob Ross on 6/9/26.
//

#include "roblib/string_builder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "roblib/allocator.h"
#include "roblib/roblib_types.h"


/*
 * Todo this code currently uses malloc/calloc when allocating buffers. Add the ability to configure a custom allocator
 * to use for allocations.
 *
 *
 */

// we want our buffer to be around 75% capacity on average to allow quicker appends
constexpr double FILL_RATIO = 3.0 / 4.0 ;
constexpr double CAPACITY_FACTOR = 1.0 / FILL_RATIO;

//// ------------------------------------------------------------
////
////    Create, Destroy functions
////
//// ------------------------------------------------------------

StringBuilder * sb_init( StringBuilder *sb, uint32_t capacity, char const * str) {
    uint32_t str_len = strlen(str);
    uint32_t max_len = (str_len < capacity) ? capacity : str_len;

    uint32_t ideal_capacity = (uint32_t)(max_len * CAPACITY_FACTOR);
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
    sb->arena = nullptr;
    return sb;
}

void sb_destroy(StringBuilder *sb) {
    if (sb->arena) {
        free(sb->buffer);
        sb->buffer = nullptr;
    }
    sb->capacity = 0;
    sb->length = 0;
}

StringBuilder * sb_from_cstring( char const * cstring, uint32_t capacity, AlokArena *arena ) {
    const size_t str_sz = strlen(cstring);
    const uint32_t str_len =  (str_sz >= U32_MAX) ? U32_MAX - 1 : strlen(cstring);
    const uint32_t max_len = (str_len < capacity) ? capacity : str_len;

    uint32_t ideal_capacity = (uint32_t)(max_len * CAPACITY_FACTOR);
    if (ideal_capacity == 0) ideal_capacity = 16;
    if ( capacity < ideal_capacity ) capacity = ideal_capacity;

    StringBuilder * sb   = alok_arena_alloc( arena, sizeof(StringBuilder), alignof(StringBuilder), nullptr );
    char          * buf  = alok_arena_alloc( arena, (capacity + 1) * sizeof(char), alignof(char),  nullptr );
    if (!buf || !sb) {
        // todo (rob) error reporting
        return nullptr;
    }
    sb->buffer = buf;
    memcpy(sb->buffer, cstring, str_len);
    sb->buffer[str_len] = '\0';
    sb->capacity = capacity;
    sb->length = str_len;
    sb->arena = arena;
    return sb;
}

StringBuilder * sb_new( const uint32_t capacity, AlokArena *arena ) {
    return sb_from_cstring("", capacity, arena);
}

static bool sb_ensure_capacity(StringBuilder *sb, uint32_t capacity_wanted) {
    if (capacity_wanted <= sb->capacity) return true;
    // must grow capacity
    uint32_t old_capacity = sb->capacity;
    uint32_t new_capacity = (uint32_t)(capacity_wanted * CAPACITY_FACTOR);
    if (old_capacity > new_capacity) {
        // capacity is at max size, buffer is full. Overflow.
        return false;
    }
    char *buf = nullptr;
    if (sb->arena) {
        buf = alok_arena_alloc(sb->arena,  new_capacity + 1, alignof(char), nullptr );
        if (!buf) return false;
    } else {
        buf = realloc(sb->buffer, new_capacity + 1);
        if (!buf) return false;
        // clear new bytes
        memset(buf + old_capacity, '\0', new_capacity - old_capacity + 1);
    }
    sb->buffer = buf;
    sb->capacity = new_capacity;
    return true;
}

StringBuilder * sb_append_char( StringBuilder *sb,  char c) {
    if (c == '\0') return sb;  // adding empty string does nothing.
    uint32_t new_length = sb->length + sizeof(char);
    //todo (rob) why are we passing new_length + 1 here??
    if (!sb_ensure_capacity(sb, new_length + 1)) return nullptr;

    sb->buffer[sb->length++] = c;
    sb->buffer[sb->length] = '\0';
    sb->length = new_length;

    return sb;
}

StringBuilder * sb_append_str(StringBuilder *sb, char const *str) {
    if (str[0] == '\0') return sb;  // adding empty string does nothing.
    uint32_t old_len = sb->length;
    uint32_t new_length = sb->length + strlen(str);
    if (!sb_ensure_capacity(sb, new_length )) return nullptr;
    strcpy(sb->buffer + old_len, str );
    sb->length = new_length;
    return sb;
}

void sb_copy_to(StringBuilder *sb, uint32_t buf_size, char out_buffer[static buf_size ]) {
    uint32_t copy_len = buf_size;
    if (sb->length < copy_len) {
        copy_len = sb->length;
    }
    strncpy(out_buffer, sb->buffer, copy_len );
    out_buffer[buf_size-1] = '\0';
}

// todo (rob) this only works if the string is only composed of ASCII characters.
// we need to make this utf-8 aware
StringBuilder * sb_insert_char( StringBuilder *sb, char c, const uint32_t index) {
    if (c == '\0') return sb;  // inserting empty string does nothing.
    uint32_t new_length = sb->length + sizeof(c);
    if (!sb_ensure_capacity(sb, new_length)) return nullptr;
    memmove((sb->buffer + index + 1), (sb->buffer + index), new_length - index);
    sb->buffer[index] = c;
    sb->length = new_length;
    return sb;
}

StringBuilder * sb_insert_str( StringBuilder *sb, char const * str, const uint32_t index) {
    if (str[0] == '\0') return sb;  // inserting empty string does nothing.
    const uint32_t str_len = strlen(str);
    const uint32_t new_length = sb->length + str_len;
    if (!sb_ensure_capacity(sb, new_length)) return nullptr;
    memmove((sb->buffer + index + str_len), (sb->buffer + index), sb->length - index);
    strncpy(sb->buffer + index, str, str_len );
    sb->length = new_length;
    return sb;
}

uint32_t sb_replace_match_chars(StringBuilder *sb, char const * match_chars, char const replacement_char) {
    // todo (rob) for long strings with a large number of match_chars, we might want to build a boolean lookup table of
    // ascii values for O(1) lookups. For now we'll try using strstr
    // this method does not change the size of the buffer

    if (!match_chars || *match_chars == '\0' || !replacement_char) {
        return 0;
    }
    char *ptr = sb->buffer;
    uint32_t num_replacements = 0;
    //optimization, if we only have one match char to check
    if (strlen(match_chars) == 1) {
        while (*ptr) {
            if (*ptr == match_chars[0] ) {
                *ptr = replacement_char;
                num_replacements++;
            }
            ptr++;
        }
    } else {
        char char_str[2] = {};  // strstr wants a char* not a single char
        while (*ptr) {
            char_str[0] = *ptr;
            if ( strstr(match_chars, char_str) ) {
                *ptr = replacement_char;
                num_replacements++;
            }
            ptr++;
        }
    }
    return num_replacements;
}

void sb_repr( StringBuilder *sb ) {
    printf("(StringBuilder){ .capacity=%d, .length=%d, .buffer=\"%s\" }\n", sb->capacity, sb->length, sb->buffer);
}

int sb_fprint( FILE* stream, StringBuilder *sb ) {
    return fprintf( stream, "%s", sb->buffer);
}

int sb_print( StringBuilder *sb ) {
    return sb_fprint(stdout, sb);
}
