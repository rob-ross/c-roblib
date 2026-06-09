// hashmap_private.h
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/03/03 19:48:12 PST

//
// Shared private API for HashMap objects
//

#pragma once

#include <stddef.h> // For size_t


size_t map_calc_bucket_index(size_t hashcode, size_t num_buckets);
MapNode * map_create_node(HashMap map[static 1], size_t hashcode, MapKey key) ;
void map_destroy_node(HashMap map[static 1], MapNode *node);
void map_ensure_capacity(HashMap *map, unsigned wanted_capacity);

bool map_equals_MapDataPolicies( MapDataPolicies o1, MapDataPolicies o2);
bool map_equals_MapKey(MapKey k1, MapKey k2);
bool map_equals_MapKeyPolicy( MapKeyPolicy kp1, MapKeyPolicy kp2) ;
bool map_equals_MapValuePolicy( MapValuePolicy vp1, MapValuePolicy vp2) ;

size_t map_hash_function(MapKey key);
MapKey map_policy_key_add_default(HashMap map[static 1], MapKey key);
void map_policy_key_free_default(HashMap map[static 1], MapKey key);
MapNode * map_node_for(const HashMap map[static 1], MapKey key);
void map_recalc_load(HashMap *map);
void map_set_value(HashMap *top_map, MapNode *node, ColValue value );


