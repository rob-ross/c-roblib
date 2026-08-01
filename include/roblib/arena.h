// arena.h
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/06/08 15:33:49 PDT
#pragma once

/**
 *  General memory management terms:
 *      Allocation: heap allocation, dynamic allocation, explicit allocation
 *          sometimes referred to as payload, chunk, anonymous memory
 *
 *      Page: The fixed-size unit (typically 4096 bytes) used by the OS to manage virtual memory.
 *      Payload: The exact usable bytes requested by the client, excluding any allocator metadata.
 *      Metadata / Header: The hidden bookkeeping bytes which are placed right before the payload
 *          to store chunk size and status.
 *      Chunk / Block: The individual, smaller segment of memory within the arena that is handed to the client user.
 *      Free List: A data structure (often a linked list) tracking all unallocated chunks inside the arena.
 *
*       Allocation size: the size of an individual, normally small, memory allocation.
 *          E.g., void * raw_ptr = malloc(10); // requesting 10 bytes, "allocating 10 bytes."
 *
 *      Arena: The large pool of memory (e.g., one or more 100 MB blocks) requested from the OS to be carved up.
 *
 *      High-Water Mark: The peak amount of total memory an allocator has claimed from the OS during execution.
 *
 *
 *      This is a simple arena/bump allocator. It obtains system memory via calls to mmap. It manages a pointer to the
 *      next available byte of unallocated memory and increments this pointer based on the requested allocation size.
 *      It will pad the request to align it to 16 byte boundaries (via _Alignof(max_align_t)). It will grow the arena
 *      dynamically if a memory request is larger than the arena capacity. The entire arena is freed with the call to
 *      arena_destroy_arena().
 *
 *
 *
 *    For POSIX we use void * mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
 *    For Windows we use malloc.
 *
 *    This API is not thread safe.
 */

#ifndef ROBLIB_ARENA_H
#define ROBLIB_ARENA_H

#include <stddef.h>

#include "error_result.h"


#ifdef __cplusplus
extern "C" {
#endif

// opaque type
typedef struct arena_s Arena;

typedef struct arena_err_result_s {
    ERROR_BASE;
    Arena * result;
} ArenaErrResult;




// must call arna_destroy() when done with the Arena
ArenaErrResult arena_create_arena( size_t arena_capacity);
void arena_reset(Arena * arena, bool zero_mem);
void arena_destroy_arena(const Arena * arena);

void * arena_alloc(Arena * arena,  size_t size);

#ifdef __cplusplus
}
#endif

#endif //ROBLIB_ARENA_H
