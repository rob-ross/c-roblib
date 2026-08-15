//  test_allocator.cpp
//
//  Created by Rob Ross on 8/14/26.
//
//  Copyright (c) 2026.  All rights reserved.

#include <gtest/gtest.h>

#include "roblib/allocator.h"

typedef unsigned char byte;

typedef struct block_header_t {
    struct block_header_t * next_block;  // Links to the next memory block
    size_t block_size;                   // Tracks size of this block for mmap
    // size available for allocations; omits BlockHeader size, other headers like AllocatorHeader, etc.
    size_t usable_size;
} BlockHeader;

typedef struct allocator_header_s {
    byte         * current_block;       // Current active memory block being filled.
    size_t         default_block_size;  // default size of each block allocation
    size_t         default_alignment;   // Alignment used for allocations in the arena when not explicitly provided
    size_t         offset;              // Position inside the *current* active block (points to next available byte)
    BlockHeader  * head_block;          // Pointer to the first block
    size_t         auto_grow;       // only used for true/false now, but preserving alignment here.
} AllocatorHeader;

typedef struct alok_arena_s {
    AllocatorHeader * const alloc_header;
} AlokArena;

typedef struct stack_marker_t {
    const BlockHeader * const mark_block; // the block in which the marker was created
    const size_t        mark_offset;      // the block offset at the time the marker was created
    bool                is_stale;         // set to true after rolling back to this mark
} StackMarker;


TEST(ArenaAllocator, alok_arena_create1) {
    ArenaErrResult aer = alok_arena_create(100);

    ASSERT_FALSE(aer.err) << "alok_arena_create returns err=false on success";
    ASSERT_NE(aer.result, nullptr) << "alok_arena_create returns new AlokArea*";
    AlokArena * arena = aer.result;
    AllocatorHeader *header = arena->alloc_header;
    BlockHeader *block1 = header->head_block;
    EXPECT_NE(header->head_block, nullptr ) << "head block is not null" ;

    //BlockHeader
    EXPECT_EQ(block1->next_block, nullptr ) << "initially the arena has a single block";
    // todo (rob) get these numbers dynamically
    EXPECT_EQ(block1->block_size, alok_align_up(100, 4096) ) << "block size for 100 byte arena is platform pagesize ";
    EXPECT_EQ(block1->usable_size, 4016) << "usable size is block size - metadata header size ";

    //AllocatorHeader
    EXPECT_EQ(header->current_block, (byte*)block1) << "initial current block is first block " ;
    EXPECT_EQ(header->default_block_size, 4096) << "default blocksize for 100 byte arena is platform pagesize" ;
    EXPECT_EQ(header->default_alignment, DEFAULT_ALIGNMENT) << "default alignment size used when not explicitly specified" ;
    EXPECT_EQ(header->offset, 80) << "initial offset is 80" ;
    EXPECT_EQ(header->auto_grow, true) << "default grow value is true when not explicitly specified" ;

    alok_arena_destroy(arena);
}

TEST(ArenaAllocator, alok_arena_alloc1) {
    ArenaErrResult aer = alok_arena_create(100);
    ASSERT_FALSE(aer.err) << "alok_arena_create returns err=false on success";
    ASSERT_NE(aer.result, nullptr) << "alok_arena_create returns new AlokArea*";
    AlokArena * arena = aer.result;

    AllocatorHeader *header = arena->alloc_header;
    BlockHeader *block1 = header->head_block;
    byte * current_ptr = (byte*)block1 + header->offset;

    // allocate 10 bytes with 1 byte alignment
    void * mem = alok_arena_alloc(arena, 10, 1);
    EXPECT_NE(mem, nullptr) << "arena alloc should return a valid pointer to allocated memory";
    EXPECT_EQ(mem, current_ptr) << "mem pointer is the block address plus current offset";

    EXPECT_EQ(header->offset, 90) << "alloc of 10 bytes moves offest by 10 bytes";


    alok_arena_destroy(arena);
}

TEST(ArenaAllocator, alok_arena_pop_to_marker1) {
    ArenaErrResult aer = alok_arena_create(100);
    AlokArena * arena = aer.result;

    AllocatorHeader *header = arena->alloc_header;
    BlockHeader *block1 = header->head_block;
    // allocate 10 bytes with 1 byte alignment
    size_t original_offset = header->offset;

    std::cout << "original offset: " << original_offset << '\n';

    void * mem = alok_arena_alloc(arena, 10, 1);
    std::cout << "offset after alloc 10: " << header->offset << '\n';

    EXPECT_EQ(header->offset, original_offset + 10) << "alloc of 10 bytes moves offest by 10 bytes";

    StackMarker *marker =  alok_arena_marker(arena);

    std::cout << "offset after making marker: " << header->offset << '\n';


    EXPECT_EQ(marker->mark_block, block1) << "marker should point to current block";
    EXPECT_EQ(marker->mark_offset, original_offset + 10) << "marker offset should match current arena offset";
    EXPECT_EQ(marker->is_stale, false) << "is_stale is false for fresh marker";

    // allocate 20 more bytes past the current marker
    void * new_mem1 = alok_arena_alloc(arena, 10, 1);
    std::cout << "offset after alloc 10: " << header->offset << '\n';

    void * new_mem2 = alok_arena_alloc(arena, 10, 1);
    std::cout << "offset after alloc 10: " << header->offset << '\n';

    EXPECT_EQ(header->offset,
        alok_align_up(original_offset + 10 , alignof(StackMarker) ) + sizeof(StackMarker) + 20)
    << "alloc of 20 bytes moves offest by 20 bytes";

    alok_arena_pop_to_marker(arena, marker);
    EXPECT_EQ(marker->is_stale, true) << "is_stale is true after popping to marker";
    EXPECT_EQ(header->offset, original_offset + 10) << "offset restored to its previous value when marker was created";

    alok_arena_destroy(arena);

}

TEST(ArenaAllocator, alok_arena_get_scratch) {
    AlokArenaTemp scratch_arena = alok_arena_get_scratch(nullptr, 0);
    AlokArena * arena = scratch_arena.arena;

    ASSERT_NE(arena, nullptr) << "alok_arena_get_scratch returns a valid arena";

    AllocatorHeader *header = arena->alloc_header;
    BlockHeader *block1 = header->head_block;
    EXPECT_NE(header->head_block, nullptr ) << "head block is not null" ;

    //BlockHeader
    EXPECT_EQ(block1->next_block, nullptr ) << "initially the arena has a single block";
    // todo (rob) get these numbers dynamically
    EXPECT_EQ(block1->block_size, alok_align_up(ALOK_DEFAULT_SCRATCH_ARENA_SIZE, 4096) )
    << "block size for temp arena is " <<  ALOK_DEFAULT_SCRATCH_ARENA_SIZE;
    EXPECT_EQ(block1->usable_size, 4016) << "usable size is block size - metadata header size ";

    //AllocatorHeader
    size_t start_offset = 80; // fragile!
    size_t aligned_marker_start = alok_align_up(start_offset  , alignof(StackMarker) ) ;
    size_t marker_padding = aligned_marker_start - start_offset;
    EXPECT_EQ(header->current_block, (byte*)block1) << "initial current block is first block " ;
    EXPECT_EQ(header->default_block_size, alok_align_up(ALOK_DEFAULT_SCRATCH_ARENA_SIZE, 4096)) << "default blocksize for scratch arena is platform pagesize" ;
    EXPECT_EQ(header->default_alignment, DEFAULT_ALIGNMENT) << "default alignment size used when not explicitly specified" ;
    EXPECT_EQ(header->offset, aligned_marker_start + sizeof(StackMarker) ) << "initial offset is 140" ;
    EXPECT_EQ(header->auto_grow, true) << "default grow value is true when not explicitly specified" ;

    size_t block_offset = header->offset;
    byte * current_ptr = (byte*)block1 + header->offset;
    // allocate 10 bytes with 1 byte alignment
    void * mem = alok_arena_alloc(arena, 10, 1);
    EXPECT_NE(mem, nullptr) << "arena alloc should return a valid pointer to allocated memory";
    EXPECT_EQ(mem, current_ptr) << "mem pointer is the block address plus current offset";

    EXPECT_EQ(header->offset, block_offset + 10) << "alloc of 10 bytes moves offest by 10 bytes";

    alok_arena_release_scratch(&scratch_arena);

    //offset should now be before the StackMarker position and the 10 bytes we allocated.
    EXPECT_TRUE(scratch_arena.marker->is_stale) << "marker should be marked stale after release";
    EXPECT_EQ(header->offset, block_offset - sizeof(StackMarker) - marker_padding);
}
