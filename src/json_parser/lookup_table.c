//  lookup_table.c
//
// Created by Rob Ross on 6/13/26.
//

#include "lookup_table.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>




/**
 * Initializes the map with the capacity given by the argument. Uses an Arena
 * for allocations. This map has no destructor. When the Arena is destroyed, the memory
 * used by the lookup table is freed.
 *
 * @param map The struct holding the context for the map
 * @param key_type Currently either char const * or long
 * @param initial_capacity The starting capacity of the map.
 * @param arena An initialized Arena
 */
void lkup_map_init(SimpleMap *map, enum lkup_key_type key_type,  int initial_capacity, Arena * arena) {
    map->key_type = key_type;
    map->capacity = initial_capacity;
    map->length = 0;
    map->entries = arena_alloc(arena, sizeof(KVEntry) * map->capacity);
}

// Simple linear search for a key
// Returns the index if found, -1 otherwise
static int lkup_map_find_index_for_string_key(SimpleMap *map, const char *key) {
    for (int i = 0; i < (int)map->length; i++) {
        if (strcmp(map->entries[i].char_key, key) == 0) {
            return i;
        }
    }
    return -1;
}

// Returns the index if found, -1 otherwise
static int lkup_map_find_index_for_long_key(SimpleMap *map, long key) {
    for (int i = 0; i < (int)map->length; i++) {
        if (map->entries[i].long_key == key) {
            return i;
        }
    }
    return -1;
}


static KVEntry * lkup_realloc(SimpleMap *map, uint32_t new_capacity, Arena * arena) {
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

// update the value at the index or add new value to map
static int lkup_map_set(SimpleMap *map, const int index, void *value, Arena * arena) {
    if (index != -1) {
        // Update existing
        map->entries[index].value = value;
        return -1;
    }

    // Check if we need to grow
    if (map->length >= map->capacity) {
        const uint32_t new_capacity = map->capacity *= 2;
        KVEntry * new_entries = lkup_realloc(map, new_capacity, arena );
        if (! new_entries) {
            fprintf(stderr, "lkup_map_set(): Error: couldn't allocate memory to resize lookup table.\n");
            return -2;
        }
        map->capacity = new_capacity;
        map->entries = new_entries;
    }
    map->entries[map->length].value = value;
    return index;

}

void lkup_map_set_for_string_key(SimpleMap *map, const char *key, void *value, Arena * arena) {
    int index = lkup_map_find_index_for_string_key(map, key);
    int result = lkup_map_set(map, index, value, arena);

    if (result >= 0) {
        // Insert new
        map->entries[map->length].char_key = key;
        map->length++;
    }
}

void lkup_map_set_for_long_key(SimpleMap *map, long key, void *value, Arena * arena) {
    int index = lkup_map_find_index_for_long_key(map, key);
    int result = lkup_map_set(map, index, value, arena);

    if (result >= 0) {
        // Insert new
        map->entries[map->length].long_key = key;
        map->length++;
    }
}

// Retrieve a value
void* lkup_map_get_for_string_key(SimpleMap *map, const char *key) {
    int index = lkup_map_find_index_for_string_key(map, key);
    return (index != -1) ? map->entries[index].value : nullptr;
}

void* lkup_map_get_for_long_key(SimpleMap *map, long key) {
    int index = lkup_map_find_index_for_long_key(map, key);
    return (index != -1) ? map->entries[index].value : nullptr;
}

// Delete a value using "swap & pop"
void lkup_map_remove_string_key(SimpleMap *map, const char *key) {
    int index = lkup_map_find_index_for_string_key(map, key);
    if (index == -1) return;

    // Move the last element into this slot, then decrement length
    if (index < (int)map->length - 1) {
        map->entries[index] = map->entries[map->length - 1];
    }
    map->length--;
}

// Delete a value using "swap & pop"
void lkup_map_remove_long_key(SimpleMap *map, long key) {
    int index = lkup_map_find_index_for_long_key(map, key);
    if (index == -1) return;

    // Move the last element into this slot, then decrement length
    if (index < (int)map->length - 1) {
        map->entries[index] = map->entries[map->length - 1];
    }
    map->length--;
}
