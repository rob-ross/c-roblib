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
 *      arena_bump_destroy().
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
// Map the C11 keyword to the native C++ keyword
#define _Alignof alignof
#endif

// opaque type
typedef struct allocator_header_s Arena;

typedef struct arena_err_result_s {
    ERR_FIELDS_UNION;
    Arena * result;
} ArenaErrResult;

typedef struct stack_allocator_s StackAllocator;
typedef struct stack_allocator_err_result_s {
    ERR_FIELDS_UNION;
    StackAllocator * result;
} StackAllocatorErrResult;


constexpr size_t MAX_ALIGNMENT     = _Alignof(max_align_t);
constexpr size_t POINTER_ALIGNMENT = _Alignof(void*);
constexpr size_t DEFAULT_ALIGNMENT = MAX_ALIGNMENT;



// must call arena_bump_destroy() when done with the Arena
ArenaErrResult arena_bump_create( size_t arena_capacity );
void arena_bump_reset(Arena * arena, bool zero_mem);
void arena_bump_destroy(const Arena * arena);


/**
 * @brief Allocates memory from the arena.
 *
 * This function can be called with 2 - 4 arguments. The `ArenaErrResult` and `align_size` parameters are optional.
 * If passed and an error occurs, it will contain the error information.
 * If no error occurs, a pointer to the newly allocated memory is returned,
 * and aer->err (if not null) will be set to false.
 * Otherwise, a nullptr is returned, and aer->err (if not null) will be set to true.
 * If `align_size` is omitted, DEFAULT_ALIGNMENT is used. Otherwise, it is used to align the memory location at which
 * the allocation is made.
 * - `arena_alloc( arena, size_t size)`
 * - `arena_alloc( arena, size_t size, [[nullable]] ArenaErrResult * aer)`
 * - `arena_alloc( arena, size_t size, [[nullable]] ArenaErrResult * aer, size_t align_size)`
 * @returns void * to the newly allocated memory
 */
#define arena_bump_alloc(_1, _2, ...) \
    _arena_bump_alloc_SELECT_(__VA_ARGS__ __VA_OPT__(,) _arena_bump_alloc_4, _arena_bump_alloc_3, _arena_bump_alloc_2)(_1, _2 __VA_OPT__(,) __VA_ARGS__)

// --- Internal Use Only ---

#define _arena_bump_alloc_2(_1, _2)        (_arena_bump_alloc)(_1, _2, nullptr, DEFAULT_ALIGNMENT)
#define _arena_bump_alloc_3(_1, _2, _3)    (_arena_bump_alloc)(_1, _2, _3,      DEFAULT_ALIGNMENT)
#define _arena_bump_alloc_4(_1, _2, _3, _4)(_arena_bump_alloc)(_1, _2, _3,      _4)
#define _arena_bump_alloc_SELECT_(_1, _2, NAME, ...) NAME
void * _arena_bump_alloc(Arena * arena,  size_t size, [[nullable]] ArenaErrResult * aer, size_t alignment); // NOLINT(*-reserved-identifier)



//// ------------------------------------------------------------
////
////        STACK ALLOCATOR
////
//// ------------------------------------------------------------

StackAllocatorErrResult arena_stack_create( size_t capacity) ;



#ifdef __cplusplus
#undef _Alignof // Clean up the macro
}
#endif

#endif //ROBLIB_ARENA_H
