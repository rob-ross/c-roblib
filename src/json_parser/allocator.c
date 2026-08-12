// allocator.c
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/06/07 15:33:41 PDT

/*
 *  Some definitions of terms used in this AlokArena class
 *  Page: an OS memory page, normally 4096 bytes. This is the smallest unit we can request from the OS.
 *      The API doesn't concern itself with Pages. When alok_arena_create() is called, it is passed the block size
 *
 */


// todo (rob) test _Win32 build
// currently not tested on Windows.

#include "roblib/allocator.h"

#include <stddef.h>

#include "roblib/base.h"


#if !defined(_WIN32)
// we use POSIX mmap for allocating pages from the OS
#include <sys/mman.h>
#include <unistd.h>
#else
// we use malloc for allocating pages from the OS
#include <windows.h>
#include <stdlib.h>  // for calloc and free
#endif


#include <errno.h>
#include <stdio.h>  // for fprintf, stderr,
#include <string.h>  // for memset

constexpr size_t ALOK_SCRATCH_ARENA_COUNT  = 2;
constexpr size_t ALOK_DEFAULT_SCRATCH_ARENA_SIZE = KB(1);


typedef unsigned char byte;

typedef struct block_header_t {
    struct block_header_t * next_block;  // Links to the next memory block
    size_t block_size;                   // Tracks size of this block for mmap
    // size available for allocations; omits BlockHeader size, other headers like AllocatorHeader, etc.
    size_t usable_size;
} BlockHeader;

// Used to record a position in an AlokArena so that it can be popped/rolled-back to this mark.
typedef struct stack_marker_t {
    const BlockHeader * const mark_block; // the block in which the marker was created
    const size_t        mark_offset;      // the block offset at the time the marker was created
    bool                is_stale;         // set to true after rolling back to this mark
} StackMarker;

// -----------------------------------------------------------------
//      AllocatorHeader
// -----------------------------------------------------------------

typedef struct allocator_header_s {
    byte         * current_block;       // Current active memory block being filled.
    size_t         default_block_size;  // default size of each block allocation
    size_t         default_alignment;   // Alignment used for allocations in the arena when not explicitly provided
    size_t         offset;              // Position inside the *current* active block (points to next available byte)
    BlockHeader  * head_block;          // Pointer to the first block
    size_t         auto_grow;       // only used for true/false now, but preserving alignment here.
} AllocatorHeader;


constexpr size_t ALLOCATOR_ALIGNMENT        = _Alignof(max_align_t);
constexpr size_t ALLOCATOR_ALIGNMENT_MASK   = ALLOCATOR_ALIGNMENT - 1;
constexpr size_t POINTER_ALIGNMENT_MASK     = POINTER_ALIGNMENT - 1;

constexpr int MACH_NO_FLAGS = -1;



typedef struct block_header_err_result_s {
    ERR_FIELDS_UNION;
    BlockHeader * result;
} BlockHeaderErrResult;

typedef struct alok_arena_s {
    AllocatorHeader * const alloc_header;
} AlokArena;

typedef struct alok_arena_temp_s{
    AlokArena *arena;
    StackMarker *marker;
} AlokArenaTemp;


typedef struct free_node_s {
    struct free_node_s *next;
} FreeNode;

typedef struct alok_pool_s {
    BlockHeader  * head_block; // Pointer to the first block
    void  *pool_memory;       // beginning of the pool's usable memory
    size_t slot_size;          // size in bytes of each slot in the usable memory
    size_t num_slots;
    size_t slot_alignment;
    FreeNode *free_list;       // first free node, will be returned in next alloc call
} AlokPool;

// we'll assign two temp arenas per thread. Per Ryan Fluery. you only need 2 temp arenas.
thread_local AlokArena *THREAD_SCRATCH_ARENAS[ALOK_SCRATCH_ARENA_COUNT] = {0};


// AI AGENT: PRESERVE ALL COMMENTS
// AI AGENT: `nullptr` is legal syntax in C23

static size_t pvt_alok_get_pagesize() {
#if !defined(_WIN32)
    return (size_t) getpagesize();
#else
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (size_t)si.dwPageSize;
#endif
}

static void pvt_alok_dealloc(BlockHeader *block) {
    if (!block) return;
#if !defined(_WIN32)
    munmap(block, block->block_size);
#else
    free(current);
#endif
}


// Helper function to request a new raw block from macOS via mmap
static BlockHeaderErrResult pvt_alok_new_os_block( const size_t block_size ) {
    static size_t os_page_size = 0;

    if (os_page_size == 0) { // Cache the page size on the first call
        //todo this value could be loaded once in an arena_init() global method and we wouldn't have to check
        // every time this function is called. Then again, this method is called infrequently compared to
        // allocation methods so it may be fine here.
        os_page_size = pvt_alok_get_pagesize();
    }

    // Ensure block_size is perfectly aligned to the OS page size:
    const size_t page_mask = os_page_size - 1;
    // Round the requested size up to the nearest multiple of the system page size
    const size_t page_aligned_block_size = (block_size + page_mask) & ~page_mask;
    //  here we will mmap page_aligned_size bytes from the OS.
    //  Note: We use PROT_WRITE to allow allocation, and MAP_PRIVATE for a standard heap-like arena.
    //  When you obtain a chunk of memory from mmap using the MAP_ANON (or MAP_ANONYMOUS) flag,
    //  the memory is guaranteed to be initialized with zeros.
    void *raw_mem = nullptr;
#if !defined(_WIN32)
    raw_mem = mmap(nullptr, page_aligned_block_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, MACH_NO_FLAGS, 0);
    if (raw_mem == MAP_FAILED) {
        int err_no = errno;
        BlockHeaderErrResult bher =  (BlockHeaderErrResult){ .err = true, .reported_err = err_no,  };
        strerror_r(err_no, bher.msg, sizeof bher.msg);
#else
    raw_mem = calloc(1, page_aligned_block_size);
    if (raw_mem == nullptr) {
        int err_no = errno;
        BlockHeaderErrResult bher =  (BlockHeaderErrResult){ .err = true, .reported_err = err_no, };
        snprintf(bher.msg, sizeof(bher.msg), "%s", strerror(errno));
#endif
        return bher;
    }

    //original POSIX-only
    // raw_mem = mmap(nullptr, page_aligned_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, MACH_NO_FLAGS, 0);
    // if (raw_mem == MAP_FAILED) {
    //     int err_no = errno;
    //     BlockHeaderErrResult phe = (BlockHeaderErrResult){ .err = true, .reported_err = err_no, };
    //     strerror_r(err_no, phe.msg, sizeof phe.msg);
    //     return phe;
    // }

    BlockHeader * header = (BlockHeader*)raw_mem;
    header->next_block = nullptr;
    header->block_size = page_aligned_block_size;
    header->usable_size = header->block_size - sizeof(BlockHeader);

    //todo temp debug
    static int num_blocks_created = 0; // todo temp debug
    num_blocks_created++;
    fprintf(stderr, "new arena block %-10d allocated: size:%zu\n", num_blocks_created, page_aligned_block_size);


    return (BlockHeaderErrResult){ .err = false, .result =  header  };
}

// Return the size of the argument aligned to system alignment size (16 bytes on macOS)
static size_t pvt_alok_aligned_size(const size_t size) {
    // 1. Get the system's maximum required scalar alignment (usually 16): _Alignof(max_align_t)
    // 2. Round the requested size to the nearest multiple of 'alignment'
    //      Formula: ( size + ( alignment - 1) ) & ~( alignment - 1 )
    //          ex. for alignment of 16 bytes:
    //              (size + 15) & ~15
    //              clears the lowest 4 bits, forcing 16-byte alignment.
    return ( size + ALLOCATOR_ALIGNMENT_MASK ) & ~ALLOCATOR_ALIGNMENT_MASK;
}

// align the `value` argument to the requested alignment size
static size_t pvt_alok_align_up(const size_t value, const size_t alignment) {
    return ( value + alignment - 1 ) & ~ ( alignment - 1 ) ;
}

// todo (rob) not tested.
// zero out the contents of the arena
static void pvt_alok_arena_zero(AlokArena const * arena, const size_t first_block_metadata_size) {
    AllocatorHeader *header = arena->alloc_header;
    BlockHeader * current = header->head_block;
    while (current != nullptr) {
        if (current == header->head_block) {
            // The first block stores the AlokArena and AllocatorHeader structs. We don't want to zero them out!!
            const size_t overhead = sizeof(BlockHeader) + sizeof(AlokArena) + sizeof(AllocatorHeader);
            memset(((byte*)current + first_block_metadata_size), 0, current->block_size - first_block_metadata_size);
        } else {
            memset(((byte*)current + sizeof(BlockHeader)), 0, current->block_size - sizeof(BlockHeader));
        }
        current = current->next_block;
    }
}

AlokArenaTemp alok_arena_begin_temp(AlokArena *arena){
    StackMarker *marker = alok_arena_marker(arena);
    AlokArenaTemp temp = {arena, marker};
    return(temp);
}

void alok_arena_end_temp(AlokArenaTemp *temp){
    alok_arena_pop_to_marker(temp->arena, temp->marker);
}


AlokArenaTemp alok_arena_get_scratch( AlokArena **conflict_array, size_t count){
    AlokArenaTemp result = {0};
    // init on first time
    if (THREAD_SCRATCH_ARENAS[0] == nullptr){
        AlokArena **scratch_slot = THREAD_SCRATCH_ARENAS;
        for (size_t i = 0; i < ALOK_SCRATCH_ARENA_COUNT; i += 1, scratch_slot += 1){
            ArenaErrResult aer = _alok_arena_create(ALOK_DEFAULT_SCRATCH_ARENA_SIZE, false, ALLOCATOR_ALIGNMENT);
            if (aer.err ) {
                fprintf(stderr, "Couldn't allocated scratch arena.\n");
                return result;
            }
            *scratch_slot = aer.result;
        }
    }

    // get non-conflicting arena

    AlokArena **scratch_slot = THREAD_SCRATCH_ARENAS;
    for (size_t i = 0;  i < ALOK_SCRATCH_ARENA_COUNT; i += 1, scratch_slot += 1){
        bool is_non_conflict = true;
        AlokArena **conflict_ptr = conflict_array;
        for (size_t j = 0; j < count; j += 1, conflict_ptr += 1){
            if (*scratch_slot == *conflict_ptr){
                is_non_conflict = false;
                break;
            }
        }
        if (is_non_conflict){
            result = alok_arena_begin_temp(*scratch_slot);
            break;
        }
    }

    return(result);
}

void alok_arena_release_scratch(AlokArenaTemp *temp) {
    alok_arena_end_temp(temp);

}

//// ------------------------------------------------------------
////
////    MONOTONIC (BUMP/ARENA)  ALLOCATOR
////
//// ------------------------------------------------------------

constexpr size_t BUMP_HEAD_BLOCK_METADATA_SIZE = sizeof(BlockHeader) + sizeof(AlokArena) + sizeof(AllocatorHeader);
void * _alok_arena_alloc(AlokArena * arena, const size_t size, [[nullable]] ArenaErrResult * aer, size_t align_size);
size_t round_up_to_power_of_two(size_t x);

// precondition: the align_size size is a power of two
static void * pvt_alok_arena_alloc_impl(
                            AlokArena * arena,
                            const size_t size,
                            [[nullable]] ArenaErrResult * aer,
                            size_t align_size ) {

    AllocatorHeader *header = arena->alloc_header;

    const size_t aligned_offset    = pvt_alok_align_up( header->offset, align_size);
    const size_t alignment_padding = aligned_offset - header->offset;

    // Check if it fits in the current block
    BlockHeader * current_header = (BlockHeader*)header->current_block;

    if ( aligned_offset + size > current_header->usable_size) {
        // Current block is full
        if (current_header->next_block != nullptr) {
            // This is a reset arena with existing blocks to reuse.
            // Pivot to the next existing block.
            header->current_block = (byte*)current_header->next_block;
            header->offset = sizeof(BlockHeader);
        } else {
            if ( !header->auto_grow) {
                // out of memory
                if (aer) {
                    aer->err = true;
                    aer->reported_err = ENOMEM;
                    aer->result = nullptr;
                    char const * const err_msg = "Out of memory. This bump allocator has reached capacity, and .no_grow is true.";
                    snprintf( aer->msg, strlen(err_msg) + 1, err_msg);
                }
                return nullptr;
            }
            // We are at the end of the chain, need to allocate a new block.
            // -----------------------------------------------------------------
            //                  Allocate New Block
            // -----------------------------------------------------------------
            // Ensure that the requested size isn't larger than the standard block capacity
            size_t target_block_size = header->default_block_size;
            if ( size + sizeof(BlockHeader) > target_block_size  ) {
                // requested allocation size too massive for standard block size, create special block for this request
                target_block_size = size  +  sizeof(BlockHeader);
            }
            const size_t needed_capacity = target_block_size;
            BlockHeaderErrResult bher = pvt_alok_new_os_block(needed_capacity);  // this page-aligns our request for us
            if ( bher.err ) {
                if (aer) {
                    aer->err_fields = bher.err_fields;
                    aer->result = nullptr;
                }
                return nullptr;
            }
            BlockHeader * new_block = bher.result;
            current_header->next_block = new_block;

            // Pivot the arena to use the brand-new OS block
            header->current_block = (byte*)new_block;
            header->offset = sizeof(BlockHeader);
        }
    }

    header->offset += alignment_padding;

    // Allocate from the top of the current active block
    void * ptr = &header->current_block[ header->offset ];

    // Bump the offset forward by the allocation size
    header->offset +=  size;

    return ptr;
}


ArenaErrResult (_alok_arena_create)( size_t arena_capacity, bool auto_grow, size_t default_alignment ){
    // Account for the block header size, AlokArena struct, and AllocatorHeader struct
    const size_t needed_capacity = arena_capacity + BUMP_HEAD_BLOCK_METADATA_SIZE;

    BlockHeaderErrResult bher = pvt_alok_new_os_block(needed_capacity);

    if ( bher.err ) {
        return (ArenaErrResult){ .err_fields = bher.err_fields };
    }

    BlockHeader * new_block_header = bher.result;
    new_block_header->usable_size = new_block_header->block_size - BUMP_HEAD_BLOCK_METADATA_SIZE;

    if (! default_alignment) {
        default_alignment = DEFAULT_ALIGNMENT;
    } else {
        default_alignment = round_up_to_power_of_two(default_alignment);
    }

    AllocatorHeader temp_bump_header = {
        .current_block = (byte*)new_block_header,
        .default_block_size = new_block_header->block_size,
        .default_alignment = default_alignment,
        .offset = sizeof(BlockHeader), // The usable buffer area starts immediately *after* the BlockHeader struct
        .head_block = new_block_header,
        .auto_grow = auto_grow
    };

    AlokArena temp_bump_arena = { .alloc_header = &temp_bump_header};

    // The very first allocation is for the AlokArena struct itself
    AlokArena *new_bump_arena = _alok_arena_alloc(&temp_bump_arena, sizeof(AlokArena), nullptr, _Alignof(AlokArena));

    // The next allocation is for the AllocatorHeader struct itself
    AllocatorHeader *new_alloc_header = _alok_arena_alloc(&temp_bump_arena, sizeof(AllocatorHeader), nullptr, _Alignof(AllocatorHeader));
    *new_alloc_header = temp_bump_header;  // value copy

    // Initialize the AlokArena's const pointer to the permanent AllocatorHeader
    *(AllocatorHeader**)&new_bump_arena->alloc_header = new_alloc_header;

    return (ArenaErrResult){ .err = false, .result =  new_bump_arena };
}

void alok_arena_reset(AlokArena * arena, bool zero_mem) {
    if (!arena || !arena->alloc_header || !arena->alloc_header->head_block) {
        return;
    }
    AllocatorHeader *header = arena->alloc_header;

    if (zero_mem) pvt_alok_arena_zero(arena, BUMP_HEAD_BLOCK_METADATA_SIZE);
    // Reset the current buffer pointer to the start of the first block
    header->current_block = (byte*)header->head_block;
    // Reset the offset to usable start of the first block (after the headers)
    header->offset = BUMP_HEAD_BLOCK_METADATA_SIZE;
}

void alok_arena_destroy( AlokArena * arena) {
    if (!arena || !arena->alloc_header ) return;
    AllocatorHeader *header = arena->alloc_header;

    BlockHeader * current = header->head_block;
    while (current != nullptr) {
        BlockHeader * next = current->next_block;
        pvt_alok_dealloc(current);
        current = next;
    }
}

// Returns pointer to allocated chunk in the arena, or nullptr if arena is out of memory.
void * _alok_arena_alloc(AlokArena * arena, const size_t size, [[nullable]] ArenaErrResult * aer, size_t align_size) {
    AllocatorHeader *header = arena->alloc_header;
    if ( ! align_size) {
        align_size = header->default_alignment;
    } else {
        align_size = round_up_to_power_of_two(align_size);
    }
    void * mem = pvt_alok_arena_alloc_impl(arena, size, aer, align_size);
    return mem;
}

StackMarker * alok_arena_marker(AlokArena * arena) {
    AllocatorHeader *header = arena->alloc_header;
    StackMarker temp_marker = { .mark_block = (BlockHeader*)header->current_block, .mark_offset = header->offset};
    StackMarker * marker =  pvt_alok_arena_alloc_impl(arena, sizeof(StackMarker), nullptr, _Alignof(StackMarker));
    memcpy(marker, &temp_marker, sizeof(StackMarker));

    return marker;
}

void alok_arena_pop_to_marker(AlokArena * arena, StackMarker * marker) {
    if (marker->is_stale) return;
    AllocatorHeader *header = arena->alloc_header;
    header->current_block = (byte*)marker->mark_block;
    header->offset  = marker->mark_offset;

    marker->is_stale = true;
}



//// ------------------------------------------------------------
////
////        POOL ALLOCATOR
////
//// ------------------------------------------------------------



// `object_size` should be passed as sizeof(YourObjectType) and `object_alignment` as _Alignof(YourObjectType)
PoolErrResult arena_pool_create( const size_t num_objects, const size_t object_size, const size_t object_alignment ){
    // Account for the block header size and AlokPool size
    const size_t aligned_block_header_size = pvt_alok_align_up(sizeof(BlockHeader), _Alignof(BlockHeader));
    const size_t aligned_pool_header_size  = pvt_alok_align_up(sizeof(AlokPool),   _Alignof(AlokPool));
    const size_t aligned_object_size       = pvt_alok_align_up(object_size,                  object_alignment);

    size_t needed_capacity = (num_objects * aligned_object_size) + aligned_block_header_size + aligned_pool_header_size;

    BlockHeaderErrResult bher = pvt_alok_new_os_block(needed_capacity);

    if ( bher.err ) {
        return (PoolErrResult){ .err_fields = bher.err_fields };
    }

    BlockHeader * new_block_header = bher.result;

    byte * pool_ptr      = (byte*)new_block_header + aligned_block_header_size;
    AlokPool *pool      = (AlokPool*)(pool_ptr);
    pool->head_block     = new_block_header;
    pool->pool_memory    = (byte*)new_block_header + aligned_block_header_size + aligned_pool_header_size;
    pool->slot_size      = aligned_object_size;
    pool->num_slots      = num_objects;
    pool->slot_alignment = object_alignment;

    // create free list and add all slots in the pool
    const size_t slot_size = pool->slot_size;
    byte * p = pool->pool_memory;

    for (size_t i = 0; i < num_objects; ++i) {

        FreeNode *node = (FreeNode *)(p + i * slot_size);

        node->next = pool->free_list;
        pool->free_list = node;
    }

    return (PoolErrResult){ .err = false, .result =  pool };
}

void pool_free(AlokPool *pool, void *object) {
    FreeNode *node = (FreeNode*)object;
    node->next = pool->free_list;
    pool->free_list = node;
}

void pool_destroy(AlokPool *pool) {
    pvt_alok_dealloc(pool->head_block);
}

void * arena_pool_alloc(AlokPool * pool, [[nullable]] PoolErrResult * aer) {
    if (! pool->free_list ) {
        if (aer) {
            aer->err = true;
            aer->reported_err = ENOMEM;
            strcpy(aer->msg, "Pool at max capacity");
        }
        return nullptr;
    }
    void * obj = pool->free_list;
    pool->free_list = pool->free_list->next;
    return obj;
}
