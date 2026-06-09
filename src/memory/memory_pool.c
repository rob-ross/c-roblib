// memory_pool.c
//
// Copyright (c) Rob Ross 2026.
//
//

#include "memory_pool.h"
#include <stdlib.h>
#include <string.h>




// --- Helper Functions ---

static MemoryPage *create_page(size_t size) {
    MemoryPage *page = (MemoryPage *)calloc(1, sizeof(MemoryPage));
    if (!page) return nullptr;

    page->start = calloc(1, size);
    if (!page->start) {
        free(page);
        return nullptr;
    }

    page->size = size;
    page->used = 0;
    page->next = nullptr;
    return page;
}

static void destroy_page(MemoryPage *page) {
    if (page) {
        free(page->start);
        free(page);
    }
}




//// ------------------------------------------------------------
////
////    Default memory policy functions
////
//// ------------------------------------------------------------


// standard malloc methods

void * mem_mempolicy_default_malloc( void* context, const size_t num_bytes ) {
    // return calloc( 1, num_bytes );
    return malloc( num_bytes );
}

void * mem_mempolicy_default_calloc( void* context, const size_t element_count, const size_t element_size ) {
    return calloc(element_count, element_size);
}

void * mem_mempolicy_default_realloc( void* context, void * pointer, const size_t old_byte_count, const size_t new_byte_count ) {
    return realloc( pointer, new_byte_count );
}

void mem_mempolicy_default_free( void* context, void * bytes ) {
    free( bytes) ;
}

// allocator methods

void * mem_mempolicy_default_allocator_alloc( void* context, size_t num_bytes ) {
    MemoryPool *pool = context;
    return pool_alloc(pool, num_bytes);
}

void * mem_mempolicy_default_allocator_calloc( void* context, const size_t element_count, const size_t element_size  ) {
    MemoryPool *pool = context;
    size_t alloc_size = element_count * element_size;
    void *bytes =  pool_alloc(pool, alloc_size);
    if (!bytes) return nullptr;

    memset(bytes, 0, alloc_size);
    return bytes;
}
void * mem_mempolicy_default_allocator_realloc( void* context, void * pointer, const size_t old_byte_count, const size_t new_byte_count  ) {
    MemoryPool *pool = context;
    void *bytes = pool_alloc(pool, new_byte_count);
    if (!bytes) return nullptr;
    memset(bytes, 0, new_byte_count);
    memcpy(bytes, pointer, old_byte_count);
    return bytes;
}

void  mem_mempolicy_default_allocator_free( void * context, void * bytes ) {
    MemoryPool *pool = context;
    pool_free(pool, bytes);
}

void mem_mempolicy_default_allocator_free_context(void * context) {
    MemoryPool *pool = context;
    pool_destroy(pool);
}

const MemPolicy MEM_DEFAULT_MALLOC_POLICY = (MemPolicy){
    .context = nullptr,
    .policy_type = MEM_POLICY_MALLOC_OWN,
    .alloc = mem_mempolicy_default_malloc,
    .calloc = mem_mempolicy_default_calloc,
    .realloc = mem_mempolicy_default_realloc,
    .free = mem_mempolicy_default_free,
};

const MemPolicy MEM_DEFAULT_ALLOCATOR_POLICY = (MemPolicy){
    .context = nullptr,  // context gets filled in by actual memory_pool
    .policy_type = MEM_POLICY_ALLOCATOR_OWN,
    .alloc = mem_mempolicy_default_allocator_alloc,
    .calloc = mem_mempolicy_default_allocator_calloc,
    .realloc = mem_mempolicy_default_allocator_realloc,
    .free = mem_mempolicy_default_allocator_free,
    .free_context = mem_mempolicy_default_allocator_free_context

};

constexpr MemPolicy NULL_MEM_POLICY = (MemPolicy){ .policy_type = MEM_POLICY_NULL };


//// ------------------------------------------------------------
////
////    Public API Implementation
////
//// ------------------------------------------------------------

void * mem_alloc_bytes(const MemPolicy mem_policy, const size_t num_bytes) {
    return mem_policy.alloc(mem_policy.context, num_bytes);
}

void * mem_calloc_bytes( MemPolicy mem_policy,  size_t element_count, size_t element_size) {
    return mem_policy.calloc(mem_policy.context, element_count, element_size);
}

void * mem_realloc_bytes( MemPolicy mem_policy, void * pointer,  size_t old_byte_count, size_t new_byte_count) {
    return mem_policy.realloc(mem_policy.context, pointer, old_byte_count, new_byte_count);
}

char * mem_strdup(MemPolicy mem_policy, char const * string) {
    const size_t str_len = strlen(string)+1;
    char *dupe = mem_alloc_bytes(mem_policy, strlen(string)+1);
    memcpy(dupe, string, str_len);
    return dupe;
}
bool mem_equals_MemPolicy(const MemPolicy o1, const MemPolicy o2) {
    if ( o1.policy_type == o2.policy_type && o1.context == o2.context && o1.alloc == o2.alloc &&
        o1.calloc == o2.calloc && o1.realloc == o2.realloc && o1.free == o2.free &&
        o1.free_context == o2.free_context) {
        return true;
    }

    return false;
}

void  mem_free_bytes(const MemPolicy mem_policy, void * bytes) {
    mem_policy.free(mem_policy.context, bytes);
}

// Creates a new memory pool and attaches it to a default allocator MemPolicy.
// Uses MEM_DEFAULT_ALLOCATOR_POLICY as the prototype. The policy_type is MEM_POLICY_ALLOCATOR_OWN.
// Because of this, any collection for which this pool is attached will destroy the pool when that collection is
// destroyed.
// This is intended to be a convenience method when creating collections (HashMap, StringCounter, Set, etc).
// The returned value of this function can be used as the MemPolicy argument
// in the various collections create() functions.
MemPolicy mem_create_default_allocator(const size_t default_page_size) {
    size_t initial_pool_size = default_page_size < DEFAULT_PAGE_SIZE ? DEFAULT_PAGE_SIZE : default_page_size;
    MemoryPool *pool = pool_create(initial_pool_size);
    if (!pool) return NULL_MEM_POLICY;

    MemPolicy mem_policy = MEM_DEFAULT_ALLOCATOR_POLICY;
    mem_policy.context = pool;
    return mem_policy;
}


MemoryPool *pool_create(size_t default_page_size) {
    default_page_size = default_page_size > DEFAULT_PAGE_SIZE ? default_page_size : DEFAULT_PAGE_SIZE;
    MemoryPool *pool = (MemoryPool *)calloc(1, sizeof(MemoryPool));
    if (!pool) return nullptr;

    pool->default_page_size = default_page_size;
    pool->pages_head = nullptr;
    return pool;
}

void pool_destroy(MemoryPool *pool) {
    if (!pool) return;

    MemoryPage *current = pool->pages_head;
    while (current) {
        MemoryPage *next = current->next;
        destroy_page(current);
        current = next;
    }
    free(pool);
}

void *pool_alloc(MemoryPool *pool, size_t size) {
    if (!pool || size == 0) return nullptr;

    // Try to allocate from existing pages
    MemoryPage *current = pool->pages_head;
    while (current) {
        if (current->size - current->used >= size) {
            void *ptr = (char *)current->start + current->used;
            current->used += size;
            return ptr;
        }
        current = current->next;
    }

    // No suitable page found, allocate a new one
    size_t new_page_size = (size > pool->default_page_size) ? size : pool->default_page_size;
    MemoryPage *new_page = create_page(new_page_size);
    if (!new_page) return nullptr;

    // Add new page to the head of the list
    new_page->next = pool->pages_head;
    pool->pages_head = new_page;

    // Allocate from the new page
    void *ptr = new_page->start;
    new_page->used = size;
    return ptr;
}

void pool_reset(MemoryPool *pool) {
    if (!pool) return;

    MemoryPage *current = pool->pages_head;
    while (current) {
        current->used = 0;
        current = current->next;
    }
}

void pool_free(MemoryPool *pool, void *ptr) {
    // In this simple pool allocator, individual frees are not supported.
    // The memory is freed when the pool is destroyed or reset.
    (void)pool;
    (void)ptr;
}
