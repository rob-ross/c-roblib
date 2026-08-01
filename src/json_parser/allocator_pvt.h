//  allocator_pvt.h
//
//  Created by Rob Ross on 7/31/26.
//
//  Copyright (c) 2026.  All rights reserved.

//

#ifndef C_ROBLIB_ALLOCATOR_PVT_H
#define C_ROBLIB_ALLOCATOR_PVT_H

#ifdef __cplusplus
extern "C" {
#endif

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
} Arena;


constexpr size_t ALLOCATOR_ALIGNMENT = _Alignof(max_align_t);
constexpr size_t ALLOCATOR_ALIGNMENT_MASK = ALLOCATOR_ALIGNMENT - 1;
constexpr int MACH_NO_FLAGS = -1;


#ifdef __cplusplus
}
#endif

#endif //C_ROBLIB_ALLOCATOR_PVT_H
