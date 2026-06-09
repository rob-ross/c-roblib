// memory_pool.h
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/03/03 15:26:49 PST

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Any object that allocates objects dynamically can be assigned a MemPolicy.
 * Initially, we have HashMap and ArrayList that use a MemPolicy.
 * The same MemPolicy object can be assigned to multiple objects. In this case, they are all part of the same
 * object graph rooted in the context of the shared MemPolicy. All allocations by objects using the same MemPolicy
 * are allocated in the same backing memory pool, either the standard system OS memory manager, accessed via the
 * stdlib methods malloc, calloc, realloc, and free. Or via a MemoryPool object which can be thought of as an allocator
 * or arena. If multiple objects share the same MemPolicy.context, exactly one may use an *_OWN policy type.
 * All others must use the corresponding *_SHARED type.
 *
 * A MemPolicy must have a single owner. If the same MemPolicy is assigned to more than one object, only one object
 * must have a policy_type of OWN, and the other objects using the same MemPolicy must have a policy_type of SHARED.
 * The object with a MemPolicyType of OWN is resoponsible for disposing the MemPolicy context. For an allocator such as
 * MemoryPool, this is an allocator/arena in which memory allocations have been made by any objects using the same
 * allocator. Care must be taking to not dispose the object that owns the MemPolicy if other objects are using any
 * memory allocated by the MemPolicy allocator. Conversely, a complex object graph can be disposed of virtually instantly
 * by disposing the owning object. This frees the MemoryPool and destroys every object in that object graph.
 *
 * A MemPolicy has two basic types, MALLOC and ALLOCATOR. If using MEM_POLICY_MALLOC_OWN or MEM_POLICY_MALLOC_SHARED,
 * calls to the memory_pool API methods mem_alloc_bytes, mem_calloc_bytes, mem_realloc_bytes, mem_strdup, and
 * mem_free_bytes will use the stdlib calls malloc, calloc, realloc, and free. mem_strdup allocates memory using malloc.
 *
 * If using MEM_POLICY_ALLOCATOR_OWN or MEM_POLICY_ALLOCATOR_SHARED, all memory allocations are dispatched to the
 * underlying MemoryPool object.
 *
 * Two default MemPolicy objects are defined, MEM_DEFAULT_MALLOC_POLICY and MEM_DEFAULT_ALLOCATOR_POLICY. To use either
 * as an object's alloctor/arena, copy the relevent MemPolicy. When using the MEM_DEFAULT_ALLOCATOR_POLICY, you must
 * also attach a MemoryPoool as the MemPolicy.context object. A convenience method, mem_create_default_allocator(),
 * has been defined for this purpose. It will create a new MemoryPool and wrap it in a MEM_DEFAULT_ALLOCATOR_POLICY,
 * setting its context member to the newly allocated MemoryPool. You can then use this MemPolicy as an argument to
 * a collection create() function. Note that this default MemPolicy is set to the MemPolicyType of
 * MEM_POLICY_ALLOCATOR_OWN. If you intend to use this same MemPolicy and MemoryPool with multiple objects/functions,
 * you must set the policy_type to MEM_POLICY_ALLOCATOR_SHARED for those objects' copies of the MemPolicy. Only
 * a single object must be responsible for disposing of the MemPolicy's MemoryPool context when that object is disposed.
 *
 * When using MEM_DEFAULT_MALLOC_POLICY, there is typically no context set, i.e., the context member is NULL. However,
 * users are free to store any object they wish, and the MemPolicy's free_context method will be called for that context when
 * the owning object is disposed. As with MEM_DEFAULT_ALLOCATOR_POLICY, only a single object should OWN this MemPolicy,
 * and any other objects sharing the same policy should have their policy_type set to MEM_POLICY_MALLOC_SHARED.
 *
 * A new MemPolicy instance (not the defaults discussed above) will have it's policy_type set to  MEM_POLICY_NONE
 * indicating the policy_type has not yet been set. Some functions returning a MemPolicy may set policy_type to NULL
 * to indicate an error occurred or to indicate no MemPolicy exists.
 *
 */
typedef enum MemPolicyType: uint8_t {
    MEM_POLICY_NONE,
    MEM_POLICY_MALLOC_OWN,
    MEM_POLICY_MALLOC_SHARED,
    // ALLOCATOR_OWN: all memory relevant to
    MEM_POLICY_ALLOCATOR_OWN,
    MEM_POLICY_ALLOCATOR_SHARED,
    MEM_POLICY_NULL,
} MemPolicyType;

// --- Structures ---

// MemPolicy struct  is a textbook "Polymorphic Allocator" interface.
// It uses function pointers (alloc, calloc, realloc, free) and a context to allow different implementations
// to sit behind the same API.

// 56 bytes
typedef struct MemPolicy {
    void * context;
    void * (*alloc)(   void * context, size_t num_bytes );
    void * (*calloc)(  void * context, size_t element_count, size_t element_size);
    void * (*realloc)( void * context, void * pointer, size_t old_byte_count, size_t new_byte_count );
    void   (*free)(    void * context, void * pointer );
    void   (*free_context)(void * context );
    MemPolicyType policy_type;
} MemPolicy;

// A single page of memory.
// 32 bytes
typedef struct MemoryPage {
    void *start;                 // Pointer to the start of the page's memory.
    size_t size;                 // Total size of this page.
    size_t used;                 // How much of this page is currently used.
    struct MemoryPage *next;     // Next page in the linked list.
} MemoryPage;

// A memory pool that manages multiple pages.
// 16 bytes
typedef struct MemoryPool {
    MemoryPage *pages_head;      // Head of the linked list of all allocated pages.
    size_t default_page_size;    // Default size for new pages.
} MemoryPool;

// static constexpr size_t DEFAULT_PAGE_SIZE = 4096;


static constexpr size_t DEFAULT_PAGE_SIZE = 64L * 1024 * 1024; // 64 MB

extern const MemPolicy MEM_DEFAULT_MALLOC_POLICY;
extern const MemPolicy MEM_DEFAULT_ALLOCATOR_POLICY;
extern const MemPolicy NULL_MEM_POLICY;
// on my system, MAX_MALLOC_BYTES = 1'099'511'627'776; // 16^10
// 2^34, ~17.2GB which is more than my computer ram
// let's clamp to something more reasonable like 2^33 bytes, about 8.6GB, half my RAM
[[maybe_unused]]
static constexpr size_t MAX_MALLOC_BYTES = 1L << 33;  // 2^33, ~8.6GB, half my RAM

// --- API Functions ---

/*
* Standard API Conventions: Arenas usually have a simpler, specific API:
1. arena_init(buffer, size): Create the arena from a backing buffer.
2. arena_alloc(arena, size): Bump the pointer and return the address.
3. arena_reset(arena): Move the pointer back to the start (effectively freeing everything).
4. arena_temp_begin(arena) / arena_temp_end(arena, temp): A convention for taking a "snapshot" of the arena's state to perform temporary work and then rewinding it only partially.
*/

// Calls the mem_policy's `alloc` function
void * mem_alloc_bytes( MemPolicy mem_policy,  size_t num_bytes);
void * mem_calloc_bytes( MemPolicy mem_policy,  size_t element_count, size_t element_size);
void * mem_realloc_bytes( MemPolicy mem_policy, void * pointer,  size_t old_byte_count, size_t new_byte_count);
char * mem_strdup(MemPolicy mem_policy, char const * string) ;
// Calls the mem_policy's `free` function
void  mem_free_bytes( MemPolicy mem_policy, void * bytes);

MemPolicy mem_create_default_allocator( size_t default_page_size);

bool mem_equals_MemPolicy( MemPolicy o1,  MemPolicy o2);
/**
 * @brief Creates a new memory pool.
 *
 * @param default_page_size The default size for new pages allocated by the pool.
 *                          If 0, a reasonable default (e.g., 4KB) is used.
 * @return A pointer to the newly created MemoryPool, or NULL on failure.
 */
MemoryPool *pool_create(size_t default_page_size);

/**
 * @brief Destroys a memory pool and frees all associated memory.
 *
 * @param pool The memory pool to destroy.
 */
void pool_destroy(MemoryPool *pool);

/**
 * @brief Allocates a block of memory from the pool.
 *
 * This function will attempt to find space in existing pages. If no space is
 * available, it will allocate a new page. For allocations larger than the
 * default page size, a new page of the required size will be allocated.
 *
 * @param pool The memory pool to allocate from.
 * @param size The number of bytes to allocate.
 * @return A pointer to the allocated memory, or NULL on failure.
 */
void *pool_alloc(MemoryPool *pool, size_t size);

/**
 * @brief Resets the memory pool, effectively "freeing" all memory.
 *
 * This function does not actually free the allocated pages but marks them as
 * available for new allocations. This is a very fast way to "clear" the pool.
 *
 * @param pool The memory pool to reset.
 */
void pool_reset(MemoryPool *pool);

/**
 * @brief Frees a specific allocation.
 *
 * Note: In this simple pool allocator, individual "free" operations are complex
 * and can lead to fragmentation. The primary "free" mechanism is `pool_reset`
 * or `pool_destroy`. This function is a placeholder for a more advanced
 * implementation and currently does nothing.
 *
 * @param pool The memory pool.
 * @param ptr Pointer to the memory to free.
 */
void pool_free(MemoryPool *pool, void *ptr);
