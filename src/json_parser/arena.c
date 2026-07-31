// arena.c
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/06/07 15:33:41 PDT

// todo (rob) WINDOWS_PORT
// todo (rob) we can create an implementation with the same API for Windows that uses malloc instead of mmap.
// until we can build out some windows stub libraries.
//

#include "roblib/arena.h"

#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>


#include <errno.h>
#include <stdio.h>
#include <string.h>

constexpr size_t ALIGNMENT = _Alignof(max_align_t);
constexpr size_t ALIGNMENT_MASK = ALIGNMENT - 1;
constexpr int MACH_NO_FLAGS = -1;

// AI AGENT: PRESERVE ALL COMMENTS
// AI AGENT: `nullptr` is legal syntax in C23

// Return the size of the argument aligned to system alignment size (16 bytes on MacOS)
static size_t arena_aligned_size(const size_t size) {
    // 1. Get the system's maximum required scalar alignment (usually 16): _Alignof(max_align_t)
    // 2. Round the requested size to the nearest multiple of 'alignment'
    //      Formula: ( size + ( alignment - 1) ) & ~( alignment - 1 )
    //          ex. for alignment of 16 bytes:
    //              (size + 15) & ~15
    //              clears the lowest 4 bits, forcing 16-byte alignment.
    return ( size + ALIGNMENT_MASK ) & ~ALIGNMENT_MASK;
}


// Helper function to request a new raw block from macOS via mmap

static BlockHeaderErrResult arena_new_os_block( const size_t block_size ) {
    // Ensure block_size is perfectly aligned to the OS page size
    const size_t page_size = (size_t)getpagesize();
    const size_t page_mask = page_size - 1;
    // Round the requested size up to the nearest multiple of the system page size
    const size_t page_aligned_size = (block_size + page_mask) & ~page_mask;
    //  here we will mmap page_aligned_size bytes from the OS.
    //  Note: We use PROT_WRITE to allow allocation, and MAP_PRIVATE for a standard heap-like arena.
    //  When you obtain a chunk of memory from mmap using the MAP_ANON (or MAP_ANONYMOUS) flag,
    //  the memory is guaranteed to be initialized with zeros.
    void *raw_mem = mmap(nullptr, page_aligned_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, MACH_NO_FLAGS, 0);
    if (raw_mem == MAP_FAILED) {
        int err_no = errno;
        BlockHeaderErrResult phe =  (BlockHeaderErrResult){ .err = true, .reported_err = err_no,  };
        strerror_r(err_no, phe.msg, sizeof phe.msg);
        return phe;
    }

    BlockHeader * header = (BlockHeader*)raw_mem;
    header->next_block = nullptr;
    header->block_size = page_aligned_size;

    //todo temp debug
    fprintf(stderr, "new arena block allocated: size:%zu\n", page_aligned_size);

    return (BlockHeaderErrResult){
        .err = false,
        .result =  header
    };
}

// Returns pointer to allocated chunk in the arena, or nullptr if arena is out of memory.
void * arena_alloc(Arena * arena, const size_t size) {
    const size_t aligned_size = arena_aligned_size(size);

    // Check if it fits in the current block
    BlockHeader * current_header = (BlockHeader*)arena->current_block;

    if (arena->offset + aligned_size > current_header->block_size) { // Current block is full
        if (current_header->next_block != nullptr) {
            // This is a reset arena with existing blocks to reuse.
            // Pivot to the next existing block.
            arena->current_block = (byte*)current_header->next_block;
            arena->offset = sizeof(BlockHeader);
        } else {
            // We are at the end of the chain, need to allocate a new block.
            // Ensure that the requested size isn't larger than the standard block capacity
            size_t target_block_size = arena->default_block_size;
            if (aligned_size > target_block_size) {
                // requested chunk size too massive for standard block size, create special block for this request
                target_block_size = aligned_size;
            }
            size_t needed_capacity = target_block_size + sizeof(BlockHeader);
            BlockHeaderErrResult phe = arena_new_os_block(needed_capacity);
            if (phe.err) {
                // ReSharper disable once CppDFAUnreachableCode
                return nullptr;
            }
            // ReSharper disable once CppDFAUnreachableCode
            BlockHeader * new_block = phe.result;
            current_header->next_block = new_block;

            // Pivot the arena to use the brand-new OS block
            arena->current_block = (byte*)new_block;
            arena->offset = sizeof(BlockHeader);

        }
    }

    // Allocate from the current active block
    void * ptr = &arena->current_block[arena->offset];
    // Bump the offset forward by the aligned size
    arena->offset += aligned_size;
    return ptr;
}


ArenaErrResult arena_create_arena(Arena * arena, const size_t block_size) {
    arena->default_block_size = block_size;

    // Account for the page tracking header size
    const size_t needed_capacity = block_size + sizeof(BlockHeader);

    BlockHeaderErrResult bher = arena_new_os_block(needed_capacity);

    if ( bher.err ) {
        // ReSharper disable once CppDFAUnreachableCode
        return (ArenaErrResult){ .error = bher.error };
    }
    // ReSharper disable once CppDFAUnreachableCode

    arena->head_block = bher.result;
    // The usable buffer area starts immediately *after* the PageHeader struct
    arena->current_block = (byte*)bher.result;
    arena->offset = sizeof(BlockHeader);

    return (ArenaErrResult){
        .error = { .err = false },
        .result =  arena,
        };
}

// todo (rob) not tested.
static void arena_zero(Arena * arena) {
    BlockHeader * current = arena->head_block;
    while (current != nullptr) {
        memset(((byte*)current + sizeof(BlockHeader)), 0, current->block_size - sizeof(BlockHeader));
        current = current->next_block;
    }
}

void arena_reset(Arena * arena, bool zero_mem) {
    if (!arena || !arena->head_block) {
        return;
    }
    if (zero_mem) arena_zero(arena);
    // Reset the current buffer pointer to the start of the first page
    arena->current_block = (byte*)arena->head_block;
    // Reset the offset to usable start of the block
    arena->offset = sizeof(BlockHeader);
}

void arena_destroy_arena(Arena * arena) {
    BlockHeader * current = arena->head_block;
    while (current != nullptr) {
        BlockHeader * next = current->next_block;
        munmap(current, current->block_size);
        current = next;
    }
}


