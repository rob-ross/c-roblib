// arena.c
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/06/08 15:33:41 PDT

//
// Created by Rob Ross on 6/7/26.
//

#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>

#include "arena.h"

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
    // 1. Get the system's maxium required scalar alignment (usually 16) : _Alignof(max_align_t)
    // 2. Round the requested size to the nearest multiple of 'alignment'
    //      Formula: ( size + ( alignment - 1) ) & ~( alignment - 1 )
    //          ex. for alignment of 16 bytes:
    //              (size + 15) & ~15
    //              clears the lowest 4 bits, forcing 16-byte alignment.
    return ( size + ALIGNMENT_MASK ) & ~ALIGNMENT_MASK;
}


// Helper function to request a new raw block from MacOS via mmap

static PageHeaderError arena_new_os_block( const size_t capacity ) {
    // Ensure capacity is perfectly aligned to the OS page size
    const size_t page_size = (size_t)getpagesize();
    const size_t page_mask = page_size - 1;
    // Round the requested size up to the nearest multiple of the system page size
    const size_t page_aligned_size = (capacity + page_mask) & ~page_mask;
    // here we will mmap page_aligned_size bytes from the OS.
    // Note: We use PROT_WRITE to allow allocation, and MAP_PRIVATE for a standard heap-like arena.
    void *raw_mem = mmap(nullptr, page_aligned_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, MACH_NO_FLAGS, 0);
    if (raw_mem == MAP_FAILED) {
        int err_no = errno;
        PageHeaderError phe =  (PageHeaderError){
            .err = true, .reported_err = err_no,
        };
        strerror_r(err_no, phe.msg, sizeof phe.msg);
        return phe;
    }

    PageHeader * header = (PageHeader*)raw_mem;
    header->next_page = nullptr;
    header->page_capacity = page_aligned_size;

    return (PageHeaderError){
        .err = false,
        .result =  header
    };
}

// Returns pointer to allocated chunk in the arena, or nullptr if arena is out of memory.
void * arena_alloc(Arena * arena, const size_t size) {
    const size_t aligned_size = arena_aligned_size(size);

    // Check if it fits in the current block
    if (arena->offset + aligned_size > arena->capacity) {

        // Esure that the requested size isn't larger than the standard block capacity
        if (aligned_size > arena->capacity) {
            return nullptr; // Payload too massive for standard block capacity
        }

        // Allocate another block of the exact same capacity from the OS
        size_t needed_capacity = arena->capacity + sizeof(PageHeader);
        PageHeaderError phe = arena_new_os_block(needed_capacity);
        if (phe.err) {
            return nullptr;
        }

        PageHeader * new_block = phe.result;

        // Find the current active block header and link the new block to it
        // The header is located directly before the current_buffer pointer
        PageHeader * current_header = (PageHeader*)(arena->current_buffer - sizeof(PageHeader));
        current_header->next_page = new_block;

        // Pivot the arena to use the brand new OS block
        arena->current_buffer = (uint8_t*)new_block + sizeof(PageHeader);
        arena->offset = 0;
    }



    // Allocate from the current active block
    void * ptr = &arena->current_buffer[arena->offset];
    // Bump the offset forward by the aligned size
    arena->offset += aligned_size;
    return ptr;
}


ArenaErrResult arena_create_arena(Arena * arena, const size_t block_capacity) {
    arena->capacity = block_capacity;

    // Account for the page tracking header size
    const size_t needed_capacity = block_capacity + sizeof(PageHeader);

    PageHeaderError phe = arena_new_os_block(needed_capacity);

    if ( phe.err ) {
        return (ArenaErrResult){ .error = phe.error};
    }

    arena->head_page = phe.result;
    // The usable bufer area starts immediately *after* the PageHeader struct
    arena->current_buffer = (uint8_t*)phe.result + sizeof(PageHeader);
    arena->offset = 0;

    return (ArenaErrResult){
        .err = false,
        .result =  arena,
        };
}


void arena_destroy_arena(Arena * arena) {
    PageHeader * current = arena->head_page;
    while (current != nullptr) {
        PageHeader * next = current->next_page;
        munmap(current, current->page_capacity);
        current = next;
    }
}


