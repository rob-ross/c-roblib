//  lookup_table.c
//
// Created by Rob Ross on 6/13/26.
//

#include "lookup_table.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"


// Initialize the map
/**
 * Initializes the map with the capacity given by the argument. Uses an Arena
 * for allocations
 * @param map The struct holding the context for the map
 * @param initial_capacity The starting capacity of the map.
 * @param arena An initialized Arena
 */
void lkup_map_init(SimpleMap *map, int initial_capacity, Arena * arena) {
    map->capacity = initial_capacity;
    map->length = 0;
    map->entries = arena_alloc(arena, sizeof(KVEntry) * map->capacity);
}

// Simple linear search for a key
// Returns the index if found, -1 otherwise
static int lkup_map_find_index(SimpleMap *map, const char *key) {
    for (int i = 0; i < (int)map->length; i++) {
        if (strcmp(map->entries[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

KVEntry * lkup_realloc(SimpleMap *map, uint32_t new_capacity, Arena * arena) {
    // we can't realloc in the arena, so we just allocate new memory and
    // copy over the old data to the new allocation
    KVEntry * new_entries = (KVEntry *)arena_alloc(arena, sizeof(KVEntry) * new_capacity);
    if (!new_entries) {
        return nullptr;
    }
    size_t current_data_size = sizeof(KVEntry) * map->length;
    memcpy(new_entries, map->entries, current_data_size);
    return new_entries;
}


// Insert or update a value
void lkup_map_set(SimpleMap *map, const char *key, void *value, Arena * arena) {
    int index = lkup_map_find_index(map, key);
    if (index != -1) {
        // Update existing
        map->entries[index].value = value;
        return;
    }

    // Check if we need to grow
    if (map->length >= map->capacity) {
        const uint32_t new_capacity = map->capacity *= 2;
        KVEntry * new_entries = lkup_realloc(map, new_capacity, arena );
        if (! new_entries) {
            fprintf(stderr, "lkup_map_set(): Error: couldn't allocate memory to resize lookup table.\n");
            return;
        }
        map->capacity = new_capacity;
        map->entries = new_entries;
    }

    // Insert new
    map->entries[map->length].key   = key;
    map->entries[map->length].value = value;
    map->length++;
}

// Retrieve a value
void* lkup_map_get(SimpleMap *map, const char *key) {
    int index = lkup_map_find_index(map, key);
    return (index != -1) ? map->entries[index].value : NULL;
}

// Delete a value using "swap & pop"
void lkup_map_remove(SimpleMap *map, const char *key) {
    int index = lkup_map_find_index(map, key);
    if (index == -1) return;

    // Move the last element into this slot, then decrement length
    if (index < (int)map->length - 1) {
        map->entries[index] = map->entries[map->length - 1];
    }
    map->length--;
}

