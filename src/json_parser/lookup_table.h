// lookup_table.h
//
// Created by Rob Ross on 6/13/26.
//

/**
 *  Simple lookup table for the json_parser to use when parsing objects.
 *  Uses a fixed size array of KVElements. Lookups are O(N) (table scan),
 *  inserts and deletes are O(1). Uses an Arena for memory allocation.
 */

#pragma once

#ifndef C_ROBLIB_LOOKUP_TABLE_H
#define C_ROBLIB_LOOKUP_TABLE_H

#include <stdint.h>

#include "roblib/arena.h"

#ifdef __cplusplus
extern "C" {
#endif

enum lkup_key_type {
    LKUP_KEY_CHAR,
    LKUP_KEY_LONG
};

// Simple entry structure
typedef struct {
    union {
        const char *char_key;
        long        long_key;
    };
    void *value;
} KVEntry;

// Simple lookup table structure
typedef struct {
    KVEntry *entries;
    uint32_t length;
    uint32_t capacity;
    enum lkup_key_type key_type;
} SimpleMap;


void lkup_map_init(SimpleMap *map, enum lkup_key_type key_type,  int initial_capacity, Arena * arena);
void lkup_map_set_for_string_key(SimpleMap *map, const char *key, void *value, Arena * arena);
void lkup_map_set_for_long_key(SimpleMap *map, long key, void *value, Arena * arena);
void* lkup_map_get_for_string_key(SimpleMap *map, const char *key);
void* lkup_map_get_for_long_key(SimpleMap *map, long key);
void lkup_map_remove_string_key(SimpleMap *map, const char *key);
void lkup_map_remove_long_key(SimpleMap *map, long key);

#ifdef __cplusplus
}
#endif

#endif //C_ROBLIB_LOOKUP_TABLE_H
