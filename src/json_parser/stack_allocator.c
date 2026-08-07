//  stack_allocator.c
//
//  Created by Rob Ross on 7/31/26.
//
//  Copyright (c) 2026.  All rights reserved.

#include <stddef.h>

#include "roblib/allocator.h"
#include "allocator_pvt.h"

//
/*
 *  This will share some code and DS from arena.c.
 *
 *  pvt_arena_get_pagesize
 *  arena_new_os_block
 *  arena_aligned_size
 *  AlokArena - maybe some members, maybe we'll need to add more
 *  we'll want to be able to specify if the allocator can grow when it's out of memory, or if it's fixed size.
 *  and let's update alok_arena_create with the same ability as well.
 *
 *  similar methods of arena but must be implemented in the stack_allocator
 *  stack_allocator_create();
 *  stack_allocator_destroy();
 *  stack_allocator_reset();
 *
 *  specialized methods for stack_allocator:
 *  push(size)
 *  pop()
 *
 *  push allocates size bytes on the top of the stack and returns a pointer to the new memory
 *  pop ... in a regular stack it removes from the stack and returns it to the caller.
 *  but any pointer returned for the top stack object is fragile. it can be overwritten in the next push().
 *
 *  So a "stack allocator" is not really anything like a DS stack. It is just like an extension to our arena.
 *  It allocates memory linearly. But now we can deallocate memory linearly too, in the reverse order of the
 *  allocations. So it's useful when you have processed some data from the top of the stack and don't need it
 *  anymore. free() just decrements the memory pointer.
 *  So we need to know every allocation we make and keep track of the start pointer and the allocation size.
 *  we need this to subract from the current offset pointer.
 *
 *  So we need a data structure for this.
 *  void * alloc_ptr; // start of the allocation
 *  size_t alloc_size; // the size of the allocation. This must be the total size, if we had to add padding
 *  to the allocation for alignment purposes.
 *  this structure itself can be stored like a stack. To free the top of our stack, we pop the top alloc_ptr element.
 *  then we set the stack pointer to the value of alloc_ptr of the new top item.
 *
 *
 *
 */


    // absolute location of the start of memory. Or we could use an offset from the start of the block
    // do we need to store the block here, if we have multiple blocks? If the addresses are absolute,  no.
    // the address of the previous MemAllocation is the new top of the stack.
    // do we even need to know the size? I don't think so. So we just need a stack of pointers.
    // Alignment is 16 bytes. (but we'll get this dynamically from max_align_t)
    // in a 4096 byte page, that's a max of 256 allocations possible in a single page due to alignment.
    // So we have to be able to record a max of 256 allocations per page. That's 2048 bytes. So for every
    // memory page we need a half of a memory page for the alloc pointers. 50% overhead in the worst case scenario.

    // we need two blocks for the stack allocator DS.
    // payload data : aligned to 16 bytes
    // alloc metadata: aligned to 8 bytes.
    // We may need to tune this over time. If the user requests a 1MB stack allocator, we must also allocate 512K for the
    // pointers. If they ask for 100 MB, we have to ask for 50MB of pointer storage. This seems unreasonable. But
    // that may only *seem* that way right now. Let's try starting with 25% for the meta block.
    // User asks for 1 mb block, we also allocate 256K for the pointers.
    // We can copy the model from arena where there's a block header that acts as a linked list of blocks if we need
    // to allocate more blocks. The existing BlockHeader struct should still work for this.
    // Our StackArena struct would build on AlokArena:
    // we need the exact same data for our meta stack as the AlokArena and payload stack.

    // Like with arena_allocator, we store the AlokArena struct on the first block after the BlockHeader






