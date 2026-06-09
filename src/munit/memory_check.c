#include <stdio.h>
#include <stdlib.h>
#include "memory_check.h"

#undef malloc
#undef calloc
#undef realloc
#undef free

_Atomic int g_alloc_count = 0;  // how many allocations have been made via malloc or calloc
_Atomic size_t g_last_alloc_size = 0;   // size of most recent malloc/calloc
_Atomic size_t g_total_allocation_size = 0; // size to date of all alloca
_Atomic int g_realloc_count = 0;   // how many realloc calls have been made
_Atomic size_t g_last_realloc_size = 0;  // size of most recent realloc
_Atomic int g_free_count = 0; // how many times has free been called




void mc_display_stats() {
    printf("\ng_alloc_count: %i\ng_last_alloc_size: %zu\ng_total_allocation_size: %zu\n", g_alloc_count, g_last_alloc_size, g_total_allocation_size);
    printf("g_realloc_count: %i\ng_last_realloc_size: %zu\n", g_realloc_count, g_last_realloc_size );
    printf("g_free_count: %i\ng_alloc_count - g_free_count = %i\n", g_free_count, (g_alloc_count) - g_free_count);
}

void mc_clear_stats() {
    g_alloc_count = 0;
    g_last_alloc_size = 0;
    g_total_allocation_size = 0;
    g_realloc_count = 0;
    g_last_realloc_size = 0;
    g_free_count = 0;
}


void *mc_malloc(size_t sz) {
    void *p = malloc(sz);
    if (p) {
        g_alloc_count++;
        g_last_alloc_size = sz;
        g_total_allocation_size += sz;
    }
    return p;
}

void *mc_calloc(size_t n, size_t sz) {
    void *p = calloc(n, sz);
    if (p) {
        g_alloc_count++;
        g_last_alloc_size = sz;
        g_total_allocation_size += sz;
    }
    return p;
}



void *mc_realloc(void *old, size_t sz) {
    void *p = NULL;
    if (!old) {
        p = realloc(NULL, sz);
        if (p) {
            g_realloc_count++;
            g_last_realloc_size  = sz;
            g_total_allocation_size += sz;
        }
    } else {
        p = realloc(old, sz);
        g_realloc_count++;
        g_last_realloc_size = sz;
        // here we have no way of telling if we are growing or shrinking the existing buffer.
    }
    return p;
}

void mc_free(void *p) {
    if (p) {
        g_free_count++;
        free(p);
    }
}

int mc_free_count() {
    return g_free_count;
}

int mc_all_freed(void) {
    return g_alloc_count == g_free_count;
}

int mc_alloc_count() {
    return g_alloc_count;
}

size_t mc_last_alloc_size() {
    return g_last_alloc_size;
}

size_t mc_total_alloc_size() {
    return g_total_allocation_size;
}

int mc_realloc_count() {
    return g_realloc_count;
}

size_t mc_last_realloc_size() {
    return g_last_realloc_size;
}
