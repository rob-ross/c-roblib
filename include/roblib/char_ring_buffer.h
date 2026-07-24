//  char_ring_buffer.h
//
//  Created by Rob Ross on 7/23/26.
//

#pragma once
#ifndef C_ROBLIB_CHAR_RING_BUFFER_H
#define C_ROBLIB_CHAR_RING_BUFFER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

constexpr size_t CharRingBuffer_SIZE = 4096;

typedef struct {
    size_t length;
    size_t start_index;
    size_t end_index;
    char   buffer[CharRingBuffer_SIZE];
} CharRingBuffer;

typedef enum : long {
    CRB_ERR_BUFFER_FULL = -3,
    CRB_ERR_READ_ERR    = -2,
    CRB_ERR_EOF         = -1,
    CRB_ERR_NONE        =  0,

} CRBErrType;


/**
 * Directly fills the ring buffer from a reader function.
 * This avoids an intermediate buffer and double-copying.
 */
long sutil_fill_ring_buffer_from_reader_CharRingBuffer(CharRingBuffer *crb,
                                         long (*read_fn)(void *, unsigned char *, size_t),
                                         void *read_context);

void sutil_add_to_buffer_CharRingBuffer(CharRingBuffer *crb, size_t count, char const *src_chars);
long sutil_add_to_buffer_strict_CharRingBuffer(CharRingBuffer *crb, size_t count, char const *src_chars);
int sutil_get_next_char_CharRingBuffer(CharRingBuffer *crb);
int sutil_peek_char_CharRingBuffer(CharRingBuffer *crb, size_t offset);
int sutil_advance_buffer_CharRingBuffer(CharRingBuffer *crb, size_t byte_count);

void sutil_print_repr_CharRingBuffer(CharRingBuffer *crb);


//// ------------------------------------------------------------
////
////    CHAR_RING_BUFFER MACRO
////
//// ------------------------------------------------------------


/**
 *  Define a struct in the form of TYPENAMESIZE along with functions that take it as an argument.
 */

#define CHAR_RING_BUFFER(TYPENAME, SIZE) \
typedef struct TYPENAME##SIZE{    \
    size_t length;  \
    size_t start_index; \
    size_t end_index; \
    char buffer[SIZE];  \
} TYPENAME##SIZE;   \
    \
long sutil_fill_ring_buffer_from_reader_##TYPENAME##SIZE(TYPENAME##SIZE *crb,                                     \
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
void sutil_add_to_buffer_##TYPENAME##SIZE(TYPENAME##SIZE *crb, size_t count, char const *src_chars) {             \
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
long sutil_add_to_buffer_strict_##TYPENAME##SIZE(TYPENAME##SIZE *crb, size_t count, char const *src_chars) {      \
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
int sutil_get_next_char_##TYPENAME##SIZE(TYPENAME##SIZE *crb) {                                                   \
    if (!crb->length) return EOF;                                                                                 \
    const size_t capacity = sizeof(crb->buffer);                                                                  \
    crb->length--;                                                                                                \
    int c = (unsigned char)crb->buffer[crb->start_index];                                                         \
    crb->start_index = (crb->start_index + 1) % capacity;                                                         \
    return c;                                                                                                     \
}                                                                                                                 \
                                                                                                                  \
int sutil_peek_char_##TYPENAME##SIZE(TYPENAME##SIZE *crb, size_t offset) {                                        \
    if (offset >= crb->length) {                                                                                  \
        return EOF;                                                                                               \
    }                                                                                                             \
    const size_t capacity = sizeof(crb->buffer);                                                                  \
    size_t peek_index = (crb->start_index + offset) % capacity;                                                   \
    return (unsigned char)crb->buffer[peek_index];                                                                \
}                                                                                                                 \
                                                                                                                  \
void sutil_print_repr_##TYPENAME##SIZE(TYPENAME##SIZE *crb) {                                                     \
    constexpr size_t capacity = sizeof(crb->buffer);                                                              \
    char str_buffer[ capacity + 1 ] = {};                                                                         \
    memcpy( str_buffer, crb->buffer, capacity );                                                                  \
    printf("(%s){ .capacity=%4zd, .length=%4zd, .start_index=%4zd, .end_index=%4zd, .buffer='%s' }\n",            \
            STRINGIFY(TYPENAME##SIZE), capacity, crb->length, crb->start_index, crb->end_index, str_buffer);      \
}                                                                                                                 \






#endif //C_ROBLIB_CHAR_RING_BUFFER_H

#ifdef __cplusplus
}
#endif
