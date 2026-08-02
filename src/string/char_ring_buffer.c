//  char_ring_buffer.c
//
// Created by Rob Ross on 7/23/26.
//

#include "roblib/char_ring_buffer.h"

#include <string.h>
#include <stdio.h>

#include "roblib/allocator.h"

//// ------------------------------------------------------------
////
////    CharRingBuffer
////
//// ------------------------------------------------------------

CharRingBuffer * crb_new_CharRingBuffer(size_t capacity, Arena *arena) {
    CharRingBuffer cb = { .capacity = capacity };
    void * ptr = arena_bump_alloc(arena, sizeof(CharRingBuffer) + capacity);
    memcpy(ptr, &cb, sizeof(CharRingBuffer));
    return (CharRingBuffer*)ptr;
}


/**
 * Directly fills the ring buffer from a reader function.
 * This avoids an intermediate buffer and double-copying.
 * Returns: bytes read, 0 for EOF, -1 for read error, -2 for buffer full.
 */
long crb_fill_ring_buffer_from_reader_CharRingBuffer(CharRingBuffer *crb,
                                         long (*read_fn)(void *, unsigned char *, size_t),
                                         void *read_context) {
    const size_t capacity = crb->capacity;

    if (crb->length >= capacity) return CRB_ERR_BUFFER_FULL;

    // Calculate the largest contiguous free block starting at end_index
    size_t free_space;
    if (crb->end_index >= crb->start_index && crb->length > 0) {
        // Space from end_index to the physical end of the array
        free_space = capacity - crb->end_index;
    } else if (crb->length == 0) {
        // Buffer is empty, we can fill the whole thing
        free_space = capacity;
    } else {
        // Space from end_index to start_index (wrapped case)
        free_space = crb->start_index - crb->end_index;
    }

    if (free_space == 0) return 0;

    // Perform the read directly into the ring buffer's internal array
    long bytes_read = read_fn(read_context, (unsigned char *)&crb->buffer[crb->end_index], free_space);
    if (bytes_read < 0) return bytes_read; // Pass through reader error (e.g., -1)

    // Update indices to maintain SOT (Source of Truth)
    crb->length += (size_t)bytes_read;
    crb->end_index = (crb->end_index + bytes_read) % capacity;

    return bytes_read;
}


void crb_add_str_to_buffer_CharRingBuffer(CharRingBuffer *crb, size_t count, char const *src_chars) {
    if (count == 0) return;

    const size_t capacity = crb->capacity;

    // If we are writing more than the capacity, we only care about the last 'capacity' bytes
    if (count > capacity) {
        src_chars += (count - capacity);
        count = capacity;
    }

    for (size_t i = 0; i < count; ++i) {
        crb->buffer[crb->end_index] = src_chars[i];

        // If the buffer was already full, or if the end_index hits the start_index,
        // we are overwriting unread data. Advance start_index to keep the buffer valid.
        if (crb->length == capacity) {
            crb->start_index = (crb->start_index + 1) % capacity;
        } else {
            crb->length++;
        }

        crb->end_index = (crb->end_index + 1) % capacity;
    }
}

void crb_add_char_to_buffer_CharRingBuffer(CharRingBuffer *crb,  char src_char) {
    const size_t capacity = crb->capacity;
    crb->buffer[crb->end_index] = src_char;

    // If the buffer was already full, or if the end_index hits the start_index,
    // we are overwriting unread data. Advance start_index to keep the buffer valid.
    if (crb->length == capacity) {
        crb->start_index = (crb->start_index + 1) % capacity;
    } else {
        crb->length++;
    }
    crb->end_index = (crb->end_index + 1) % capacity;
}


/**
 * Adds characters to the buffer without overwriting existing data.
 * Returns the number of characters actually added.
 * Returns -2 if the buffer was already full and count > 0.
 */
long crb_add_str_to_buffer_strict_CharRingBuffer(CharRingBuffer *crb, size_t count, char const *src_chars) {
    const size_t capacity = crb->capacity;
    if (count > 0 && crb->length >= capacity) return CRB_ERR_BUFFER_FULL;

    size_t added = 0;
    while (added < count && crb->length < capacity) {
        crb->buffer[crb->end_index] = src_chars[added];
        crb->end_index = (crb->end_index + 1) % capacity;
        crb->length++;
        added++;
    }
    return (long)added;
}

long crb_add_char_to_buffer_strict_CharRingBuffer(CharRingBuffer *crb, char const src_char) {
    const size_t capacity = crb->capacity;
    if ( crb->length >= capacity ) return CRB_ERR_BUFFER_FULL;
    crb->buffer[crb->end_index] = src_char;
    crb->end_index = (crb->end_index + 1) % capacity;
    crb->length++;

    return (long)1;
}

// Returns -1 if no more chars to read
int crb_get_next_char_CharRingBuffer(CharRingBuffer *crb) {
    if (!crb->length) return EOF;
    const size_t capacity = crb->capacity;

    crb->length--;
    int c = (unsigned char)crb->buffer[crb->start_index];
    crb->start_index = (crb->start_index + 1) % capacity;

    return c;
}

// Returns EOF if the offset is beyond the current buffered length
int crb_peek_char_CharRingBuffer(CharRingBuffer const *crb, size_t offset) {
    if (offset >= crb->length) {
        return EOF;
    }
    const size_t capacity = crb->capacity;
    size_t peek_index = (crb->start_index + offset) % capacity;
    return (unsigned char)crb->buffer[peek_index];
}

int crb_advance_buffer_CharRingBuffer(CharRingBuffer *crb, const size_t byte_count) {
    if (!crb->length) return EOF;
    const size_t capacity = crb->capacity;
    int bytes_advanced = 0;
    if (crb->length < byte_count ) {
        bytes_advanced = (int)(byte_count - crb->length);
    } else {
        bytes_advanced = (int)byte_count;

    }
    crb->length -= bytes_advanced;
    crb->start_index = (crb->start_index + bytes_advanced) % capacity;
    return bytes_advanced;
}

void crb_print_repr_CharRingBuffer(CharRingBuffer const *crb) {
    size_t capacity = crb->capacity;
    char str_buffer[ capacity + 1 ] = {};
    memcpy( str_buffer, crb->buffer, capacity );
    printf("(CharRingBuffer){ .capacity=%4zd, .length=%4zd, .start_index=%4zd, .end_index=%4zd, .buffer='%s' }\n",
            capacity, crb->length, crb->start_index, crb->end_index, str_buffer);
}

// Print the contents of the buffer in order into the argument `buffer`
// Assumes that the `buffer` argument is large enough to contain the CharRingBuffer's contents plus the terminator.
// I.e., sizeof(*buffer) == sizeof(CharRingBuffer.buffer)
void crb_sprint_buffer_CharRingBuffer(CharRingBuffer const *crb, char *buffer) {
    const size_t capacity = crb->capacity;
    size_t index = crb->start_index;
    size_t bytes_written = 0;
    while (index < capacity && bytes_written < crb->length) {
        buffer[bytes_written++] = crb->buffer[index++];
    }
    if ( crb->start_index >= crb->end_index) {
        index = 0;
        while (index <= crb->end_index && bytes_written < crb->length) {
            buffer[bytes_written++] = crb->buffer[index++];
        }
    }
    buffer[crb->length] = '\0';
}
