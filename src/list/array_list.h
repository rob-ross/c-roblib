// array_list.h
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/03/09 17:30:57 PDT

//
// A dynamic list backed by a C-array.
//

#pragma once

#include "../collections.h"
#include "../memory/memory_pool.h"


// like HashMap, we first support long, double, string, and void*.
// like HashMap, the base List implementation is heterogenous


typedef struct List List;

typedef struct ListValuePolicy {
    // A context pointer to be passed to the policy functions.
    void* context;
    // Called when a value is added. Returns the value to be stored.
    // Can be used for copying, interning, or reference counting.
    ColValue (*on_add_value)(List *list, ColValue value);
    // Called when a value is freed.
    void (*on_free_value)(List *list, ColValue value);
    // for any specialized cleanup of the context
    void (*on_free_context)(void* context);
    ColValuePolicyType policy_type;
} ListValuePolicy;


typedef struct ListElement {
    ColValue   value;
} ListElement;


typedef struct List {
    ListElement *elements;
    size_t size;     //the current number of elements in this list.
    size_t capacity; // max number of elements this List can hold before needed to resize
    
    ListValuePolicy value_policy;
    MemPolicy       mem_policy;
    uint64_t flags; // future use
} List;


constexpr ColValue LIST_NULL_LIST_VALUE = (ColValue){.vlong = 0, .value_type = COL_TYPE_NULL};
constexpr size_t   LIST_MIN_CAPACITY = 16;
constexpr size_t   LIST_MAX_CAPACITY = 268'435'456; // 2^28 for now. todo tune this

extern const ListValuePolicy LIST_DEFAULT_VALUE_POLICY;
extern const MemPolicy       LIST_DEFAULT_MALLOC_POLICY;


// ---------------------------
// Public API methods
// ---------------------------

// adds the value to the end of the list
CollectionsError list_append(List list[static 1], ColValue value);

//Removes all the elements from this list. After call, size == 0.
void list_clear(List list[static 1]);

// Returns true if the list contains the value, otherwise returns false.
bool list_contains(List list[static 1], ColValue value);
// call list_destroy to free all resources
List * (list_create)(size_t initial_capacity, ListValuePolicy value_policy, MemPolicy mem_policy) ;
void list_destroy(List list[static 1]);

// Returns the element at the specified position in this list.
ColValue list_get(const List list[static 1], size_t index);

CollectionsError list_insert(List list[static 1], size_t index, ColValue value );
bool list_is_empty(const List list[static 1]);

//Removes the element at the specified position in this list
//Shifts any subsequent elements to the left (subtracts one from their indices).
//Returns the element that was removed from the list.
ColValue list_remove(List list[static 1], size_t index);

// Returns the number of elements in this List
size_t list_size(const List *list);
//// ---------------------------------------------
////  repr methods
//// ---------------------------------------------
void list_repr_List(const List list[static 1], bool verbose, const char* type_str);






#define list_create_0() (list_create)( 0, LIST_DEFAULT_VALUE_POLICY, MEM_DEFAULT_MALLOC_POLICY)
#define list_create_1(_1) (list_create)(_1, LIST_DEFAULT_VALUE_POLICY, MEM_DEFAULT_MALLOC_POLICY)
#define list_create_2(_1, _2) (list_create)(_1, _2, MEM_DEFAULT_MALLOC_POLICY)
#define list_create_3(_1, _2, _3) (list_create)(_1, _2, _3)
#define list_create_SELECT_(_1, _2, _3, NAME, ...) NAME
#define list_create(...) \
list_create_SELECT_(__VA_ARGS__ __VA_OPT__(,) list_create_3, list_create_2, list_create_1, list_create_0 ) (__VA_ARGS__)