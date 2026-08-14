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
 *      AlokArena: The large pool of memory (e.g., one or more 100 MB blocks) requested from the OS to be carved up.
 *
 *      High-Water Mark: The peak amount of total memory an allocator has claimed from the OS during execution.
 *
 *
 *      This is a simple arena/bump allocator. It obtains system memory via calls to mmap. It manages a pointer to the
 *      next available byte of unallocated memory and increments this pointer based on the requested allocation size.
 *      It will pad the request to align it to 16 byte boundaries (via _Alignof(max_align_t)). It will grow the arena
 *      dynamically if a memory request is larger than the arena capacity. The entire arena is freed with the call to
 *      alok_arena_destroy().
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

// opaque types
typedef struct alok_arena_s AlokArena;
typedef struct alok_pool_s  AlokPool;

typedef struct stack_marker_t StackMarker;

typedef struct alok_arena_temp_s{
    AlokArena *arena;
    StackMarker *marker;
} AlokArenaTemp;

typedef struct arena_err_result_s {
    ERR_FIELDS_UNION;
    AlokArena * result;
} ArenaErrResult;



typedef struct pool_err_result_s {
    ERR_FIELDS_UNION;
    AlokPool * result;
} PoolErrResult;

constexpr size_t MAX_ALIGNMENT     = _Alignof(max_align_t);
constexpr size_t POINTER_ALIGNMENT = _Alignof(void*);
constexpr size_t DEFAULT_ALIGNMENT = MAX_ALIGNMENT;

// todo (rob) this probably belongs in a more general utilities type unit

size_t alok_align_up(const size_t value, const size_t alignment);

//// ------------------------------------------------------------
////
////    MONOTONIC (Arena, Bump, Linear)  ALLOCATOR
////
//// ------------------------------------------------------------



/**
 *  * @brief Creates a new arena(bump) linear allocator. Caller must call `alok_arena_destroy` when finished with it.
 *
 *  This function can be called with 1-3 arguments:
 *      `arena_capacity`, `auto_grow`, and `default_alignment`.
 *
 * @param arena_capacity : The initial size in bytes of the arena. This value will be page-aligned to the system page
 * size. If the `auto_grow` argument is `false`, this is a hard limit, and when reached further allocations will fail
 * with EMEM.
 * @param auto_grow : (Optional). Default is true. When true, if the capacity is exceeded, a new block will be
 * allocated equal in size to the original capacity. The allocator will grow in size without limit.
 * @param default_alignment : (Optional) Default value is DEFAULT_ALIGNMENT, aka MAX_ALIGNMENT. All calls to
 * `alok_arena_alloc()` without providing an `alignment` argument will use this default value to align allocations.
 * @returns BumpErrResult. If `.err` is true, an error occurred. Otherwise, `.result` contains a pointer to the
 * new allocator.
 */
#define alok_arena_create(_1, ...) \
    _alok_arena_create_SELECT_(\
        __VA_ARGS__ __VA_OPT__(,) _alok_arena_create_3, _alok_arena_create_2, _alok_arena_create_1)\
                (_1 __VA_OPT__(,) __VA_ARGS__)
#define _alok_arena_create_1(_1)            (_alok_arena_create)(_1, true, DEFAULT_ALIGNMENT)
#define _alok_arena_create_2(_1, _2)        (_alok_arena_create)(_1, _2,    DEFAULT_ALIGNMENT)
#define _alok_arena_create_3(_1, _2, _3)    (_alok_arena_create)(_1, _2,    _3)
#define _alok_arena_create_SELECT_(_1, _2, NAME, ...) NAME

// todo (rob) add "zero" flag, default is true, writes 0 to memory as it is allocated
ArenaErrResult _alok_arena_create( size_t arena_capacity, bool auto_grow, size_t default_alignment );
// macro notes: in the SELECT_ parameter list, the numbers are for the OPTIONAL arguments. _1 is for no_grow,
// _2 is for default_alignment.


void alok_arena_reset( AlokArena * arena, bool zero_mem);
void alok_arena_destroy( AlokArena * arena);


/**
 * @brief Allocates memory from the arena.
 *
 * This function can be called with 2 - 4 arguments. The `align_size` and `ArenaErrResult`   parameters are optional.
 * If `aer` is not null and an error occurs, it will contain the error information.
 * If no error occurs, a pointer to the newly allocated memory is returned,
 * and aer->err (if not null) will be set to false.
 * Otherwise, a nullptr is returned, and aer->err (if not null) will be set to true.
 * If `align_size` is omitted, the default size set in `alok_arena_create` is used.
 * This defaults to DEFAULT_ALIGNMENT if not explicitly set. Otherwise, the argument value is used
 * to align the memory location at which the allocation is made.
 * - `arena_alloc( arena, size_t size)`
 * - `arena_alloc( arena, size_t size, size_t align_size)`
 * - `arena_alloc( arena, size_t size, size_t align_size, [[nullable]] ArenaErrResult * aer)`
 * @returns void * to the newly allocated memory
 */
#define alok_arena_alloc(_1, _2, ...) \
    _alok_arena_alloc_SELECT_(\
        __VA_ARGS__ __VA_OPT__(,) _alok_arena_alloc_4, _alok_arena_alloc_3, _alok_arena_alloc_2)\
            (_1, _2 __VA_OPT__(,) __VA_ARGS__)

// --- Internal Use Only ---

#define _alok_arena_alloc_2(_1, _2)        (_alok_arena_alloc)(_1, _2,  0, nullptr )
#define _alok_arena_alloc_3(_1, _2, _3)    (_alok_arena_alloc)(_1, _2, _3, nullptr )
#define _alok_arena_alloc_4(_1, _2, _3, _4)(_alok_arena_alloc)(_1, _2, _3,     _4 )
#define _alok_arena_alloc_SELECT_(_1, _2, NAME, ...) NAME

void * _alok_arena_alloc(AlokArena * arena, size_t size, size_t align_size, [[nullable]] ArenaErrResult * aer);  // NOLINT(*-reserved-identifier)


StackMarker * alok_arena_marker(AlokArena * arena);
void alok_arena_pop_to_marker(AlokArena * arena, StackMarker * marker);

AlokArenaTemp alok_arena_get_scratch(AlokArena **conflict_array, size_t count);
void alok_arena_release_scratch(AlokArenaTemp *temp);
AlokArenaTemp alok_arena_begin_temp(AlokArena *arena);
void alok_arena_end_temp(AlokArenaTemp *temp);


#ifdef __cplusplus
#undef _Alignof // Clean up the macro
}
#endif

#endif //ROBLIB_ARENA_H
