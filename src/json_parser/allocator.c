// allocator.c
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/06/07 15:33:41 PDT

/*
 *  Some definitions of terms used in this Arena class
 *  Page: an OS memory page, normally 4096 bytes. This is the smallest unit we can request from the OS.
 *      The API doesn't concern itself with Pages. When arena_bump_create() is called, it is passed the block size
 *
 */


// todo (rob) test _Win32 build
// currently not tested on Windows.

#include "roblib/allocator.h"

#include <stddef.h>


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


typedef unsigned char byte;

typedef struct block_header_t {
    struct block_header_t * next_block;  // Links to the next memory block
    size_t block_size;                   // Tracks size of this block for mmap
} BlockHeader;

// -----------------------------------------------------------------
//      Arena
// -----------------------------------------------------------------

typedef struct allocator_header_s {
    byte         * current_block;  // Current active memory block being filled.
    size_t         default_block_size;     // Size of each block allocation
    size_t         offset;         // Position inside the *current* active block (points to next available byte)
    BlockHeader  * head_block;        // Pointer to the first block
} AllocatorHeader;


constexpr size_t ALLOCATOR_ALIGNMENT        = _Alignof(max_align_t);
constexpr size_t ALLOCATOR_ALIGNMENT_MASK   = ALLOCATOR_ALIGNMENT - 1;
constexpr size_t POINTER_ALIGNMENT_MASK     = POINTER_ALIGNMENT - 1;

constexpr int MACH_NO_FLAGS = -1;

typedef struct block_header_err_result_s {
    ERR_FIELDS_UNION;
    BlockHeader * result;
} BlockHeaderErrResult;


typedef struct stack_allocator_s {
    AllocatorHeader * payload_data;
    AllocatorHeader * meta_data;
} StackAllocator;


// AI AGENT: PRESERVE ALL COMMENTS
// AI AGENT: `nullptr` is legal syntax in C23

static size_t pvt_arena_get_pagesize() {
#if !defined(_WIN32)
    return (size_t) getpagesize();
#else
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (size_t)si.dwPageSize;
#endif
}


// Helper function to request a new raw block from macOS via mmap
static BlockHeaderErrResult arena_new_os_block( const size_t block_size ) {
    static size_t os_page_size = 0;

    if (os_page_size == 0) { // Cache the page size on the first call
        //todo this value could be loaded once in an arena_init() global method and we wouldn't have to check
        // every time this function is called. Then again, this method is called infrequently compared to
        // allocation methods so it may be fine here.
        os_page_size = pvt_arena_get_pagesize();
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

    //todo temp debug
    static int num_blocks_created = 0; // todo temp debug
    num_blocks_created++;
    fprintf(stderr, "new arena block %-10d allocated: size:%zu\n", num_blocks_created, page_aligned_block_size);


    return (BlockHeaderErrResult){
        .err = false,
        .result =  header
    };
}

// Return the size of the argument aligned to system alignment size (16 bytes on macOS)
static size_t arena_aligned_size(const size_t size) {
    // 1. Get the system's maximum required scalar alignment (usually 16): _Alignof(max_align_t)
    // 2. Round the requested size to the nearest multiple of 'alignment'
    //      Formula: ( size + ( alignment - 1) ) & ~( alignment - 1 )
    //          ex. for alignment of 16 bytes:
    //              (size + 15) & ~15
    //              clears the lowest 4 bits, forcing 16-byte alignment.
    return ( size + ALLOCATOR_ALIGNMENT_MASK ) & ~ALLOCATOR_ALIGNMENT_MASK;
}

// align the offset value to the requested alignment size
static size_t arena_align_offset(const size_t offset, const size_t alignment) {
    return ( offset + alignment - 1 ) & ~ ( alignment - 1 ) ;
}

//// ------------------------------------------------------------
////
////    MONOTONIC (BUMP)  ALLOCATOR
////
//// ------------------------------------------------------------


// todo (rob) not tested.
static void arena_zero(Arena const * arena) {
    BlockHeader * current = arena->head_block;
    while (current != nullptr) {
        if (current == arena->head_block) {
            // The first block stores the Arena struct itself. We don't want to zero it out!!
            memset(((byte*)current + sizeof(BlockHeader) + sizeof(Arena)), 0, current->block_size - sizeof(BlockHeader) - sizeof(Arena));
        } else {
            memset(((byte*)current + sizeof(BlockHeader)), 0, current->block_size - sizeof(BlockHeader));
        }
        current = current->next_block;
    }
}

// precondition: the align_size size is a power of two
static void * pvt_arena_bump_alloc_impl(
                            Arena * arena,
                            const size_t size,
                            [[nullable]] ArenaErrResult * aer,
                            const size_t align_size ) {

    const size_t aligned_offset    = arena_align_offset( arena->offset, align_size);
    const size_t alignment_padding = aligned_offset - arena->offset;

    // Check if it fits in the current block
    BlockHeader * current_header = (BlockHeader*)arena->current_block;

    if ( aligned_offset + size > current_header->block_size - sizeof(BlockHeader)) {
        // Current block is full
        if (current_header->next_block != nullptr) {
            // This is a reset arena with existing blocks to reuse.
            // Pivot to the next existing block.
            arena->current_block = (byte*)current_header->next_block;
            arena->offset = sizeof(BlockHeader);
        } else {
            // todo check a "is_resizeable" flag here. If false, report EMEM error
            // We are at the end of the chain, need to allocate a new block.
            // -----------------------------------------------------------------
            //                  Allocate New Block
            // -----------------------------------------------------------------
            // Ensure that the requested size isn't larger than the standard block capacity
            size_t target_block_size = arena->default_block_size;
            // todo ASK AI I think we still need to check aligned_requested_size here
            if ( size + sizeof(BlockHeader) > target_block_size  ) {
                // requested allocation size too massive for standard block size, create special block for this request
                // target_block_size = aligned_requested_size +  sizeof(BlockHeader);
                target_block_size = size  +  sizeof(BlockHeader);
            }
            const size_t needed_capacity = target_block_size;
            BlockHeaderErrResult bher = arena_new_os_block(needed_capacity);  // this page-aligns our request for us
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
            arena->current_block = (byte*)new_block;
            arena->offset = sizeof(BlockHeader);
        }
    }

    arena->offset += alignment_padding;

    // Allocate from the top of the current active block
    void * ptr = &arena->current_block[ arena->offset ];

    // Bump the offset forward by the allocation size
    arena->offset +=  size;

    return ptr;
}

// todo (rob) add optional parameter for specifying alignment as `size_t alignment`
// must verify that the alignment size is a power of two. use platform_specific.round_up_to_power_of_two.
// Returns pointer to allocated chunk in the arena, or nullptr if arena is out of memory.
void * _arena_bump_alloc(Arena * arena, const size_t size, [[nullable]] ArenaErrResult * aer, size_t align_size) {
    return pvt_arena_bump_alloc_impl(arena, size, aer, align_size);
}

// todo (rob) optional parameter to specify the default alignment
// add optional parameter 'resizable`, defaults to true.
ArenaErrResult arena_bump_create( const size_t arena_capacity) {
    // Account for the block header size and Arena size
    const size_t needed_capacity = arena_capacity + sizeof(BlockHeader) + sizeof(Arena);

    BlockHeaderErrResult bher = arena_new_os_block(needed_capacity);

    if ( bher.err ) {
        return (ArenaErrResult){ .err_fields = bher.err_fields };
    }

    BlockHeader * new_block_header = bher.result;

    Arena arena_prototype = {};
    arena_prototype.default_block_size = new_block_header->block_size; // this has been page-aligned by arena_new_os_block();
    arena_prototype.head_block = new_block_header;
    arena_prototype.current_block = (byte*)new_block_header;
    // The usable buffer area starts immediately *after* the BlockHeader struct
    arena_prototype.offset = sizeof(BlockHeader);

    // the very first allocation is for the Arena struct itself
    Arena *new_arena = _arena_bump_alloc(&arena_prototype, sizeof(Arena), nullptr, _Alignof(Arena));
    *new_arena = arena_prototype;

    return (ArenaErrResult){
        .err_fields = { .err = false },
        .result =  new_arena,
        };
}

void arena_bump_reset(Arena * arena, bool zero_mem) {
    if (!arena || !arena->head_block) {
        return;
    }
    if (zero_mem) arena_zero(arena);
    // Reset the current buffer pointer to the start of the first block
    arena->current_block = (byte*)arena->head_block;
    // Reset the offset to usable start of the first block
    arena->offset = sizeof(BlockHeader) + sizeof(Arena);
}

void arena_bump_destroy(const Arena * arena) {
    BlockHeader * current = arena->head_block;
    while (current != nullptr) {
        BlockHeader * next = current->next_block;
#if !defined(_WIN32)
        munmap(current, current->block_size);
#else
        free(current);
#endif
        current = next;
    }
}



//// ------------------------------------------------------------
////
////        STACK ALLOCATOR
////
//// ------------------------------------------------------------

//todo (rob) test this!!
// todo (rob) optional parameter to specify the default alignment
StackAllocatorErrResult arena_stack_create( const size_t capacity) {
    // In addition to the requested capacity, The first block requires:
    //   a BlockHeader, a StackAllocator, and an AllocatorHeader for the payload block.
    // For the meta_data block, we need to calculate 25% of the capacity arg, then
    //   a BlockHeader, and the AllocatorHeader
    // If there are multiple blocks, every block after the first starts with just a BlockHeader
    // We're adding the StackAllocator to the payload block because it will have max alignment and is more
    // future-proof, if we add members to that struct. The metadata block will be aligned to 8 bytes for efficient
    // memory use so we can't add ad hoc types.

    const size_t needed_payload_capacity = capacity + sizeof(BlockHeader) + sizeof(StackAllocator) + sizeof(AllocatorHeader);
    BlockHeaderErrResult bher = arena_new_os_block(needed_payload_capacity);
    if ( bher.err ) {
        return (StackAllocatorErrResult){ .err_fields = bher.err_fields };
    }

    BlockHeader * new_block_header = bher.result;
    AllocatorHeader payload_allocator_header = {
        .default_block_size = new_block_header->block_size,
        .head_block = new_block_header,
        .current_block = (byte*)new_block_header,
        .offset = sizeof(BlockHeader)
    };
    // the very first allocation is for the StackAllocator struct itself
    StackAllocator *new_stack_allocator = _arena_bump_alloc(&payload_allocator_header, sizeof(StackAllocator), nullptr, _Alignof(StackAllocator));

    // the next allocation is for this block's allocator header
    AllocatorHeader *new_payload_allocator_header = _arena_bump_alloc(&payload_allocator_header, sizeof(AllocatorHeader), nullptr, _Alignof(AllocatorHeader));
    *new_payload_allocator_header = payload_allocator_header;

    new_stack_allocator->payload_data = new_payload_allocator_header;

    // now we create the second block for the allocation pointers
    size_t needed_metadata_capacity = (size_t)( new_block_header->block_size * 0.25L ) + sizeof(BlockHeader) + sizeof(AllocatorHeader);
    bher = arena_new_os_block(needed_metadata_capacity);
    if ( bher.err ) {
        // todo (rob) deallocate the payload block
        return (StackAllocatorErrResult){ .err_fields = bher.err_fields };
    }

    new_block_header = bher.result;
    AllocatorHeader metadata_allocator_header = {
        .default_block_size = new_block_header->block_size,
        .head_block = new_block_header,
        .current_block = (byte*)new_block_header,
        .offset = sizeof(BlockHeader)
    };
    // the very first allocation is for the AllocatorHeader struct itself
    AllocatorHeader *new_metadata_allocator_header = _arena_bump_alloc(&metadata_allocator_header, sizeof(AllocatorHeader), nullptr, _Alignof(AllocatorHeader));
    *new_metadata_allocator_header = metadata_allocator_header;

    new_stack_allocator->meta_data = new_metadata_allocator_header;


    return (StackAllocatorErrResult){ .err = false , .result =  new_stack_allocator };
}

void * stack_allocator_alloc(StackAllocator * stack_alloc, const size_t size, [[nullable]] ArenaErrResult * aer) {
    // we allocate the payload data first, then we allocate the memory for the payload pointer.
    void* payload_mem = _arena_bump_alloc(stack_alloc->payload_data, size, aer, DEFAULT_ALIGNMENT);
    // todo error checking
    // use 8-byte alignment for the pointer allocation
    void ** pointer_mem = pvt_arena_bump_alloc_impl(stack_alloc->meta_data, sizeof(void*), aer, POINTER_ALIGNMENT);
    // todo error checking
    *pointer_mem = payload_mem;
    return payload_mem;
}
