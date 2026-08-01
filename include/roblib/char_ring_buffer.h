//  char_ring_buffer.h
//
//  Created by Rob Ross on 7/23/26.
//

#pragma once
#ifndef C_ROBLIB_CHAR_RING_BUFFER_H
#define C_ROBLIB_CHAR_RING_BUFFER_H

#include <stdio.h>
#include <stdlib.h>



#ifdef __cplusplus
extern "C" {
#endif

//// ------------------------------------------------------------
////
////    CharRingBuffer API
////
//// ------------------------------------------------------------

typedef struct arena_s Arena;
constexpr size_t CharRingBuffer_SIZE = 40;

/**
 * CharRingBuffer is the "prototype" definition of a CharRingBuffer and accompanying API methods.
 * When debugging, adding, and modifying code in the macro, use the concrete version for CharRingBuffer
 * to implement and debug the final changes, then edit the macro to include the new generic code.
 * Keep the flow one-way from Concrete Implementation -> Macro code
 *
 */
typedef struct {
    size_t length;
    const size_t capacity;
    size_t start_index;
    size_t end_index;
    char   buffer[];  // flexible member array
} CharRingBuffer;

typedef enum : long {
    CRB_ERR_BUFFER_FULL = -3,
    CRB_ERR_READ_ERR    = -2,
    CRB_ERR_EOF         = -1,
    CRB_ERR_NONE        =  0,

} CRBErrType;

/**
 * Allocate a new `CharRingBuffer` in the provided Arena.
 * @param capacity the max length of this `CharRingBuffer`
 * @param arena the Arena allocator from which to allocate memory for the new CharRingBuffer
 * @return the new object as a CharRingBuffer *. It will be deallocated when
 * arena_destroy_arena()  is called on the Arena object.
 */
CharRingBuffer * crb_new_CharRingBuffer(size_t capacity, Arena *arena);

void crb_add_str_to_buffer_CharRingBuffer(CharRingBuffer *crb, size_t count, char const *src_chars);
void crb_add_char_to_buffer_CharRingBuffer(CharRingBuffer *crb,  char src_char);

long crb_add_str_to_buffer_strict_CharRingBuffer(CharRingBuffer *crb, size_t count, char const *src_chars);
long crb_add_char_to_buffer_strict_CharRingBuffer(CharRingBuffer *crb, char src_char);
int crb_get_next_char_CharRingBuffer(CharRingBuffer *crb);
int crb_peek_char_CharRingBuffer(CharRingBuffer const *crb, size_t offset);
int crb_advance_buffer_CharRingBuffer(CharRingBuffer *crb, size_t byte_count);

void crb_print_repr_CharRingBuffer(CharRingBuffer const *crb);
void crb_sprint_buffer_CharRingBuffer(CharRingBuffer const *crb, char *buffer);

//// ------------------------------------------------------------
////
////    CHAR_RING_BUFFER MACRO
////
//// ------------------------------------------------------------

// break for macro definition

/**
 *  Define a struct in the form of TYPENAMESIZE along with functions that take it as an argument.<P>
 *  E.g., <P>
 *      CHAR_RING_BUFFER(SavedChars, 40);<P>
 *      // defines:<P>
 *      tydpedef struct SavedChars40 SavedChars40;<P>
 *      // a CharRingBuffer with 40 char capacity, and a verion of each API method such as:<P>
 *      void sutil_add_to_buffer_SavedChars40(SavedChars40 *crb, size_t count, char const *src_chars);<P>
 *      ...<P>
 */

#define CHAR_RING_BUFFER(TYPENAME, SIZE) \
typedef struct TYPENAME##SIZE{           \
    size_t length;                       \
    size_t start_index;                  \
    size_t end_index;                    \
    char buffer[SIZE];                   \
} TYPENAME##SIZE;                        \
                                         \
static long pvt_crb_fill_ring_buffer_from_reader_##TYPENAME##SIZE(TYPENAME##SIZE *crb,                            \
                                         long (*read_fn)(void *, unsigned char *, size_t),                        \
                                         void *read_context) {                                                    \
    const size_t capacity = sizeof(crb->buffer);                                                                  \
    if (crb->length >= capacity) return CRB_ERR_BUFFER_FULL;                                                      \
    size_t free_space;                                                                                            \
    if (crb->end_index >= crb->start_index && crb->length > 0) {                                                  \
        free_space = capacity - crb->end_index;                                                                   \
    } else if (crb->length == 0) {                                                                                \
        free_space = capacity;                                                                                    \
    } else {                                                                                                      \
        free_space = crb->start_index - crb->end_index;                                                           \
    }                                                                                                             \
    if (free_space == 0) return 0;                                                                                \
    long bytes_read = read_fn(read_context, (unsigned char *)&crb->buffer[crb->end_index], free_space);           \
    if (bytes_read < 0) return bytes_read;                                                                        \
    crb->length += (size_t)bytes_read;                                                                            \
    crb->end_index = (crb->end_index + bytes_read) % capacity;                                                    \
    return bytes_read;                                                                                            \
}                                                                                                                 \
                                                                                                                  \
static void pvt_crb_add_str_to_buffer_##TYPENAME##SIZE(TYPENAME##SIZE *crb, size_t count, char const *src_chars) {    \
    if (count == 0) return;                                                                                       \
    const size_t capacity = sizeof(crb->buffer);                                                                  \
    if (count > capacity) {                                                                                       \
        src_chars += (count - capacity);                                                                          \
        count = capacity;                                                                                         \
    }                                                                                                             \
    for (size_t i = 0; i < count; ++i) {                                                                          \
        crb->buffer[crb->end_index] = src_chars[i];                                                               \
        if (crb->length == capacity) {                                                                            \
            crb->start_index = (crb->start_index + 1) % capacity;                                                 \
        } else {                                                                                                  \
            crb->length++;                                                                                        \
        }                                                                                                         \
        crb->end_index = (crb->end_index + 1) % capacity;                                                         \
    }                                                                                                             \
}                                                                                                                 \
                                                                                                                  \
static void pvt_crb_add_char_to_buffer_##TYPENAME##SIZE(TYPENAME##SIZE *crb,  char src_char) {                  \
    const size_t capacity = sizeof(crb->buffer);                                                                  \
    crb->buffer[crb->end_index] = src_char;                                                                       \
    if (crb->length == capacity) {                                                                                \
        crb->start_index = (crb->start_index + 1) % capacity;                                                     \
    } else {                                                                                                      \
        crb->length++;                                                                                            \
    }                                                                                                             \
    crb->end_index = (crb->end_index + 1) % capacity;                                                             \
}                                                                                                                 \
                                                                                                                  \
static long pvt_crb_add_str_to_buffer_strict_##TYPENAME##SIZE(TYPENAME##SIZE *crb, size_t count, char const *src_chars) {      \
    const size_t capacity = sizeof(crb->buffer);                                                                  \
    if (count > 0 && crb->length >= capacity) return CRB_ERR_BUFFER_FULL;                                         \
    size_t added = 0;                                                                                             \
    while (added < count && crb->length < capacity) {                                                             \
        crb->buffer[crb->end_index] = src_chars[added];                                                           \
        crb->end_index = (crb->end_index + 1) % capacity;                                                         \
        crb->length++;                                                                                            \
        added++;                                                                                                  \
    }                                                                                                             \
    return (long)added;                                                                                           \
}                                                                                                                 \
                                                                                                                  \
static long pvt_crb_add_char_to_buffer_strict_##TYPENAME##SIZE(TYPENAME##SIZE *crb, char const src_char) {      \
    const size_t capacity = sizeof(crb->buffer);                                                                  \
    if ( crb->length >= capacity ) return CRB_ERR_BUFFER_FULL;                                                    \
    crb->buffer[crb->end_index] = src_char;                                                                       \
    crb->end_index = (crb->end_index + 1) % capacity;                                                             \
    crb->length++;                                                                                                \
                                                                                                                  \
    return (long)1;                                                                                               \
}                                                                                                                 \
                                                                                                                  \
static int pvt_crb_get_next_char_##TYPENAME##SIZE(TYPENAME##SIZE *crb) {                                          \
    if (!crb->length) return EOF;                                                                                 \
    const size_t capacity = sizeof(crb->buffer);                                                                  \
    crb->length--;                                                                                                \
    int c = (unsigned char)crb->buffer[crb->start_index];                                                         \
    crb->start_index = (crb->start_index + 1) % capacity;                                                         \
    return c;                                                                                                     \
}                                                                                                                 \
                                                                                                                  \
static int pvt_crb_peek_char_##TYPENAME##SIZE(TYPENAME##SIZE *crb, size_t offset) {                               \
    if (offset >= crb->length) {                                                                                  \
        return EOF;                                                                                               \
    }                                                                                                             \
    const size_t capacity = sizeof(crb->buffer);                                                                  \
    size_t peek_index = (crb->start_index + offset) % capacity;                                                   \
    return (unsigned char)crb->buffer[peek_index];                                                                \
}                                                                                                                 \
                                                                                                                  \
static void pvt_crb_print_repr_##TYPENAME##SIZE(TYPENAME##SIZE *crb) {                                            \
    constexpr size_t capacity = sizeof(crb->buffer);                                                              \
    char str_buffer[ capacity + 1 ] = {};                                                                         \
    memcpy( str_buffer, crb->buffer, capacity );                                                                  \
    printf("(%s){ .capacity=%4zd, .length=%4zd, .start_index=%4zd, .end_index=%4zd, .buffer='%s' }\n",            \
            STRINGIFY(TYPENAME##SIZE), capacity, crb->length, crb->start_index, crb->end_index, str_buffer);      \
}                                                                                                                 \
                                                                                                                  \
static void pvt_crb_sprint_buffer_##TYPENAME##SIZE(TYPENAME##SIZE *crb, char *buffer) {                           \
    const size_t capacity = sizeof(crb->buffer);                                                                  \
    size_t index = crb->start_index;                                                                              \
    size_t bytes_written = 0;                                                                                     \
    while (index < capacity && bytes_written < crb->length) {                                                     \
        buffer[bytes_written++] = crb->buffer[index++];                                                           \
    }                                                                                                             \
    if ( crb->start_index >= crb->end_index) {                                                                     \
        index = 0;                                                                                                \
        while (index <= crb->end_index && bytes_written < crb->length) {                                          \
            buffer[bytes_written++] = crb->buffer[index++];                                                       \
        }                                                                                                         \
    }                                                                                                             \
    buffer[crb->length] = '\0';                                                                               \
}                                                                                                                 \

#ifdef __cplusplus
}
#endif

#endif //C_ROBLIB_CHAR_RING_BUFFER_H
