//
// Created by Rob Ross on 1/31/26.
//

#ifndef CS50X_MEM_UTILS_H
#define CS50X_MEM_UTILS_H

// memory_check.h

#include <stdatomic.h>
#include <stdbool.h>

extern _Atomic int g_alloc_count;  // how many allocations have been made via malloc or calloc
extern _Atomic size_t g_last_alloc_size;   // size of most recent malloc/calloc
extern _Atomic size_t g_total_allocation_size; // size to date of all alloca
extern _Atomic int g_realloc_count;   // how many realloc calls have been made
extern _Atomic size_t g_last_realloc_size;  // size of most recent realloc
extern _Atomic int g_free_count; // how many times has free been called

void mc_display_stats();
void mc_clear_stats();

void *mc_malloc(size_t sz);
void *mc_calloc(size_t n, size_t sz);
void *mc_realloc(void *old, size_t sz);
void mc_free(void *p);
int mc_free_count();
int mc_all_freed(void);
int mc_alloc_count();
size_t mc_last_alloc_size();
size_t mc_total_alloc_size();
int mc_realloc_count();
size_t mc_last_realloc_size();


#define malloc mc_malloc
#define calloc  mc_calloc
#define realloc mc_realloc
#define free mc_free
#define boot_all_freed mc_all_freed
#define boot_realloc_count mc_realloc_count
#define boot_last_realloc_size mc_last_realloc_size
#define boot_alloc_size mc_total_alloc_size

#define stats mc_display_stats
#define clear_stats mc_clear_stats

#endif //CS50X_MEM_UTILS_H