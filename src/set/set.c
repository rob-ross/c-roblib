// set.c
//
// Copyright (c) Rob Ross 2026.
//
// Created by Rob Ross on 3/3/26.
//

#include "set.h"
#include <stdlib.h>

Set set_create(const size_t num_buckets) {
    // We pass NULL for the free_value_func because the values in the map are just dummy integers
    // and don't need to be freed. The keys are managed by the HashMap itself.
    HashMap *map = map_create(num_buckets);
    Set set = { .map = map };
    return set;
}

void set_destroy(Set *set) {
    if (set && set->map) {
        map_destroy(set->map);
        set->map = nullptr;
    }
}

void set_add(const Set *set, const SetItem item) {
    // We cast to void because we don't need the return value of map_put
    (void)(map_put)(set->map, item.item, DUMMY_VALUE);
}

bool set_contains(const Set *set, const SetItem item) {
    return (map_contains_key)(set->map, item.item);
}

bool set_is_empty(const Set *set) {
    return map_is_empty(set->map);
}

void set_remove(const Set *set, const SetItem item) {
    (map_remove)(set->map, item.item);
}

size_t set_size(const Set *set) {
    return map_size(set->map);
}
