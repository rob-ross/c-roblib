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


// Simple entry structure
typedef struct {
    const char *key;
    void *value;
} KVEntry;

// Simple lookup table structure
typedef struct {
    KVEntry *entries;
    uint32_t length;
    uint32_t capacity;
} SimpleMap;




#endif //C_ROBLIB_LOOKUP_TABLE_H
