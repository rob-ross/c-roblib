// arena.h
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/06/08 15:33:49 PDT
#pragma once

/**
 *  Terms:
 *      Payload size:  The amount of memory requested in a call to malloc, e.g., malloc(sizeof(int))
 *      Chunk Size:    a chunk is the amount of memory requested in a malloc-like call, plus bookkeeping overhead, i.e.,
 *                     payload size + metadata header (16 bytes in 64-bit system)
 *      Page Size: the memory size used by the memory manager to allocate system memory to an allocator.
 *                  The memory manager organizes system memory into pages. It then reserves chunks from these pages
 *                  to satisfy allocation requests.
 *
 *      This is a simple arena/bump allocator. It obtains system memory via calls to mmap. It manages a pointer to the
 *      next available byte of unallocated memory and increments this pointer based on the requested allocation size.
 *      It will pad the request to align it to 16 byte boundaries (via _Alignof(max_align_t)). It will grow the arena
 *      dynamically if a memory request is larger than the arena capacity. The entire arena is freed with the call to
 *      destroy_arena()
 *
 *
     void *
     mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
 */

#ifndef ROBLIB_ARENA_H
#define ROBLIB_ARENA_H

#include <stdint.h>
#include <sys/_types/_size_t.h>

#include "error_result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct page_header_t {
    struct page_header_t * next_page;  // Links to the next OS memory block
    size_t page_capacity;           // Tracks capacity of this block for mmap
} PageHeader;

typedef struct arena_s {
    uint8_t    * current_buffer;  // Current active memory block being filled
    size_t       capacity;        // Size of each OS block allocation
    size_t       offset;          // Position inside the *current* active block (points to next available byte)
    PageHeader * head_page;       // Pointer to the first block
} Arena;

constexpr size_t ONE_MIBIBYTE = 1024 * 1024;
constexpr size_t ONE_GIBIBYTE = ONE_MIBIBYTE * 1024;


typedef struct arena_err_result_s {
    ERROR_BASE;
    Arena * result;
} ArenaErrResult;

typedef struct pageheader_err_result_s {
    ERROR_BASE;
    PageHeader * result;
} PageHeaderError;


ArenaErrResult arena_create_arena( Arena * arena,  size_t block_capacity);
void arena_destroy_arena( Arena * arena);
void * arena_alloc(Arena * arena,  size_t size);

#ifdef __cplusplus
}
#endif

#endif //ROBLIB_ARENA_H
