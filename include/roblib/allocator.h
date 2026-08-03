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
 *      BumpArena: The large pool of memory (e.g., one or more 100 MB blocks) requested from the OS to be carved up.
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
typedef struct allocator_header_s BumpArena;

typedef struct arena_err_result_s {
    ERR_FIELDS_UNION;
    BumpArena * result;
} BumpArenaErrResult;

typedef struct stack_arena_s StackArena;
typedef struct stack_arena_err_result_s {
    ERR_FIELDS_UNION;
    StackArena * result;
} StackArenaErrResult;

typedef struct pool_arena_s PoolArena;
typedef struct pool_arena_err_result_s {
    ERR_FIELDS_UNION;
    PoolArena * result;
} PoolArenaErrResult;

constexpr size_t MAX_ALIGNMENT     = _Alignof(max_align_t);
constexpr size_t POINTER_ALIGNMENT = _Alignof(void*);
constexpr size_t DEFAULT_ALIGNMENT = MAX_ALIGNMENT;

//// ------------------------------------------------------------
////
////    MONOTONIC (BUMP, LINEAR)  ALLOCATOR
////
//// ------------------------------------------------------------



/**
 *  * @brief Creates a new bump allocator. Caller must call `arena_bump_destroy` when finished with it.
 *
 *  This function can be called with 1-3 arguments:
 *      `arena_capacity`, `no_grow`, and `default_alignment`.
 *
 * @param arena_capacity : The initial size in bytes of the arena. This value will be page-aligned to the system page
 * size. If the `no_grow` argument is `true`, this is a hard limit, and when reached further allocations will fail
 * with EMEM.
 * @param no-grow : (Optional). Default is false. When false, if the capacity is exceeded, a new block will be
 * allocated equal in size to the original capacity. The allocator will grow in size without limit.
 * @param default_alignment : (Optional) Default value is DEFAULT_ALIGNMENT, aka MAX_ALIGNMENT. All calls to
 * `arena_bump_alloc()` without providing an `alignment` argument will use this default value to align allocations.
 * @returns BumpErrResult. If `.err` is true, an error occurred. Otherwise, `.result` contains a pointer to the
 * new allocator.
 */
#define arena_bump_create(_1, ...) \
    _arena_bump_create_SELECT_(\
        __VA_ARGS__ __VA_OPT__(,) _arena_bump_create_3, _arena_bump_create_2, _arena_bump_create_1)\
                (_1 __VA_OPT__(,) __VA_ARGS__)
#define _arena_bump_create_1(_1)            (_arena_bump_create)(_1, false, DEFAULT_ALIGNMENT)
#define _arena_bump_create_2(_1, _2)        (_arena_bump_create)(_1, _2,    DEFAULT_ALIGNMENT)
#define _arena_bump_create_3(_1, _2, _3)    (_arena_bump_create)(_1, _2,    _3)
#define _arena_bump_create_SELECT_(_1, _2, NAME, ...) NAME

BumpArenaErrResult _arena_bump_create( size_t arena_capacity, bool no_grow, size_t default_alignment );
// macro notes: in the SELECT_ parameter list, the numbers are for the OPTIONAL arguments. _1 is for no_grow,
// _2 is for default_alignment.


void arena_bump_reset( BumpArena * arena, bool zero_mem);
void arena_bump_destroy( BumpArena * arena);


/**
 * @brief Allocates memory from the arena.
 *
 * This function can be called with 2 - 4 arguments. The `BumpArenaErrResult` and `align_size` parameters are optional.
 * If passed and an error occurs, it will contain the error information.
 * If no error occurs, a pointer to the newly allocated memory is returned,
 * and aer->err (if not null) will be set to false.
 * Otherwise, a nullptr is returned, and aer->err (if not null) will be set to true.
 * If `align_size` is omitted, the default size set in `arena_bump_create` is used. This defaults to DEFAULT_ALIGNMENT
 * if not explicitly set. Otherwise, the argument value is used to align the memory location at which
 * the allocation is made.
 * - `arena_alloc( arena, size_t size)`
 * - `arena_alloc( arena, size_t size, [[nullable]] ArenaErrResult * aer)`
 * - `arena_alloc( arena, size_t size, [[nullable]] ArenaErrResult * aer, size_t align_size)`
 * @returns void * to the newly allocated memory
 */
#define arena_bump_alloc(_1, _2, ...) \
    _arena_bump_alloc_SELECT_(\
        __VA_ARGS__ __VA_OPT__(,) _arena_bump_alloc_4, _arena_bump_alloc_3, _arena_bump_alloc_2)\
            (_1, _2 __VA_OPT__(,) __VA_ARGS__)

// --- Internal Use Only ---

#define _arena_bump_alloc_2(_1, _2)        (_arena_bump_alloc)(_1, _2, nullptr, 0)
#define _arena_bump_alloc_3(_1, _2, _3)    (_arena_bump_alloc)(_1, _2, _3,      0)
#define _arena_bump_alloc_4(_1, _2, _3, _4)(_arena_bump_alloc)(_1, _2, _3,      _4)
#define _arena_bump_alloc_SELECT_(_1, _2, NAME, ...) NAME
void * _arena_bump_alloc(BumpArena * arena,  size_t size, [[nullable]] BumpArenaErrResult * aer, size_t alignment); // NOLINT(*-reserved-identifier)





//// ------------------------------------------------------------
////
////        STACK ALLOCATOR
////
//// ------------------------------------------------------------

StackArenaErrResult arena_stack_create( size_t capacity) ;



#ifdef __cplusplus
#undef _Alignof // Clean up the macro
}
#endif

#endif //ROBLIB_ARENA_H
