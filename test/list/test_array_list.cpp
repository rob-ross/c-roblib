//  test_array_list.cpp
//
//  Created by Rob Ross on 8/18/26.
//
//  Copyright (c) 2026.  All rights reserved.

//

#include "gtest/gtest.h"
#include "roblib/array_list.h"


static bool equals_ListValuePolicy(ListValuePolicy o1, ListValuePolicy o2) {
    if (o1.policy_type == o2.policy_type && o1.context == o2.context && o1.on_add_value == o2.on_add_value &&
        o1.on_free_value == o2.on_free_value && o1.on_free_context == o2.on_free_context) {
        return true;
        }
    return false;
}


TEST(ListCreate, test_create_0) {
    // test all default arguments
    List *list;
    list = list_create();
    ASSERT_NE(list, nullptr) << "list_create() returns non-null pointer";
    EXPECT_NE(list->elements, nullptr) << "list_create() returns list with non-null elements member";
    EXPECT_EQ(list->capacity, LIST_MIN_CAPACITY) << "list_create() has capacity = " << LIST_MIN_CAPACITY;
    EXPECT_EQ(list->size, 0) << "list_create() has size = 0" ;
    EXPECT_TRUE(list_is_empty(list));

    EXPECT_TRUE(equals_ListValuePolicy(list->value_policy, LIST_DEFAULT_VALUE_POLICY))
    << "list_create() uses LIST_DEFAULT_VALUE_POLICY";
    EXPECT_TRUE(mem_equals_MemPolicy( list->mem_policy, MEM_DEFAULT_MALLOC_POLICY)) ;
    list_destroy(list);
}

TEST(ListCreate, test_create_1) {
    // test explicit initial capacity, rest default args
    List *list;
    list = list_create(17); //closest next power of 2 is 32
    ASSERT_NE(list, nullptr) << "list_create(17) returns non-null pointer";
    EXPECT_NE(list->elements, nullptr) << "list_create(17) returns list with non-null elements member";
    EXPECT_EQ(list->capacity, 32) << "list_create(17) has capacity = 32";
    EXPECT_EQ(list->size, 0) << "list_create(17) has size = 0" ;
    EXPECT_TRUE(list_is_empty(list));
    EXPECT_TRUE(equals_ListValuePolicy(list->value_policy, LIST_DEFAULT_VALUE_POLICY))
    << "list_create() uses LIST_DEFAULT_VALUE_POLICY";
    EXPECT_TRUE(mem_equals_MemPolicy( list->mem_policy, MEM_DEFAULT_MALLOC_POLICY)) ;
    list_destroy(list);
}

TEST(ListCreate, test_create_2) {
    // test explicit initial capacity and value policy, rest default args
    List *list;
    list = list_create(17, LIST_DEFAULT_VALUE_POLICY); //closest next power of 2 is 32
    ASSERT_NE(list, nullptr) << "list_create(17,LIST_DEFAULT_VALUE_POLICY) returns non-null pointer";
    EXPECT_NE(list->elements, nullptr) << "list_create(17,LIST_DEFAULT_VALUE_POLICY) returns list with non-null elements member";
    EXPECT_EQ(list->capacity, 32) << "list_create(17,LIST_DEFAULT_VALUE_POLICY) has capacity = 32";
    EXPECT_EQ(list->size, 0) << "list_create(17,LIST_DEFAULT_VALUE_POLICY) has size = 0" ;
    EXPECT_TRUE(list_is_empty(list));
    EXPECT_TRUE(equals_ListValuePolicy(list->value_policy, LIST_DEFAULT_VALUE_POLICY))
    << "list_create() uses LIST_DEFAULT_VALUE_POLICY";
    EXPECT_TRUE(mem_equals_MemPolicy( list->mem_policy, MEM_DEFAULT_MALLOC_POLICY)) ;
    list_destroy(list);
}

TEST(ListCreate, test_create_3) {
    // test explicit initial capacity, value policy, and mem policy
    List *list;
    list = list_create(0, LIST_DEFAULT_VALUE_POLICY, MEM_DEFAULT_MALLOC_POLICY);
    ASSERT_NE(list, nullptr) << "list_create(0, LIST_DEFAULT_VALUE_POLICY, MEM_DEFAULT_MALLOC_POLICY) returns non-null pointer";
    EXPECT_NE(list->elements, nullptr) << "list_create(0, LIST_DEFAULT_VALUE_POLICY, MEM_DEFAULT_MALLOC_POLICY) returns list with non-null elements member";
    EXPECT_EQ(list->capacity, LIST_MIN_CAPACITY) << "list_create(0, LIST_DEFAULT_VALUE_POLICY, MEM_DEFAULT_MALLOC_POLICY) has capacity = " << LIST_MIN_CAPACITY;
    EXPECT_EQ(list->size, 0) << "list_create(0, LIST_DEFAULT_VALUE_POLICY, MEM_DEFAULT_MALLOC_POLICY) has size = 0" ;
    EXPECT_TRUE(list_is_empty(list));
    EXPECT_TRUE(equals_ListValuePolicy(list->value_policy, LIST_DEFAULT_VALUE_POLICY))
    << "list_create() uses LIST_DEFAULT_VALUE_POLICY";
    EXPECT_TRUE(mem_equals_MemPolicy( list->mem_policy, MEM_DEFAULT_MALLOC_POLICY)) ;
    list_destroy(list);
}

class ArrayListTest : public testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;
    List *list = nullptr;
};

void ArrayListTest::SetUp() {
    list = list_create(0, LIST_DEFAULT_VALUE_POLICY, MEM_DEFAULT_MALLOC_POLICY);
    ASSERT_NE(list, nullptr) << "list_create(0, LIST_DEFAULT_VALUE_POLICY, MEM_DEFAULT_MALLOC_POLICY) returns non-null pointer";
}

void ArrayListTest::TearDown() {
    list_destroy(list);
}

TEST_F(ArrayListTest, test_append) {

    CollectionsError result = list_append(list, (ColValue){.vlong = 1, .value_type = COL_TYPE_LONG});
    EXPECT_EQ(COL_OK, result) << "result code is COL_OK";
    EXPECT_EQ(list_size(list), 1) << "list size after adding 1 item is 1";


    ColValue v = list_get(list, 0);
    EXPECT_EQ(v.vlong, 1) << "value at index 0 is 1";

    result = list_append(list, (ColValue){.vlong = 2, .value_type = COL_TYPE_LONG});
    result = list_append(list, (ColValue){.vlong = 3, .value_type = COL_TYPE_LONG});
    EXPECT_EQ(list_size(list), 3) << "list size after adding 2 more items is 3";

    v = list_get(list, 1);
    EXPECT_EQ(v.vlong, 2) << "value at index 1 is 2";
    v = list_get(list, 2);
    EXPECT_EQ(v.vlong, 3) << "value at index 2 is 3";
}

TEST_F(ArrayListTest, test_clear) {

    CollectionsError result;
    result = list_append(list, (ColValue){.vlong = 1, .value_type = COL_TYPE_LONG});
    EXPECT_EQ(COL_OK, result) << "result code is COL_OK";
    result = list_append(list, (ColValue){.vlong = 2, .value_type = COL_TYPE_LONG});
    EXPECT_EQ(COL_OK, result) << "result code is COL_OK";
    result = list_append(list, (ColValue){.vlong = 3, .value_type = COL_TYPE_LONG});
    EXPECT_EQ(COL_OK, result) << "result code is COL_OK";

    EXPECT_EQ(list_size(list), 3) << "list size after appending 3 items is 3";

    list_clear(list);

    EXPECT_EQ(list_size(list), 0) << "list size after calling list_clear() is 0";
    EXPECT_TRUE(list_is_empty(list)) << "list_is_empty() returns true after list_clear()";
}

TEST_F(ArrayListTest, test_contains) {

    CollectionsError result;
    result = list_append(list, (ColValue){.vlong = 1, .value_type = COL_TYPE_LONG});
    EXPECT_EQ(COL_OK, result) << "result code is COL_OK";
    result = list_append(list, (ColValue){.vlong = 2, .value_type = COL_TYPE_LONG});
    EXPECT_EQ(COL_OK, result) << "result code is COL_OK";
    result = list_append(list, (ColValue){.vlong = 3, .value_type = COL_TYPE_LONG});
    EXPECT_EQ(COL_OK, result) << "result code is COL_OK";

    EXPECT_TRUE(list_contains(list, (ColValue){.vlong = 2L, .value_type = COL_TYPE_LONG}))
    << "list_contains() returns true for value 2";
}

TEST_F(ArrayListTest, test_get) {
    CollectionsError result;
    result = list_append(list, (ColValue){.vlong = 1, .value_type = COL_TYPE_LONG});
    result = list_append(list, (ColValue){.vlong = 2, .value_type = COL_TYPE_LONG});
    result = list_append(list, (ColValue){.vlong = 3, .value_type = COL_TYPE_LONG});
    EXPECT_EQ(COL_OK, result) << "result code is COL_OK";

    EXPECT_EQ( list_size(list), 3 ) << "size after adding 3 elements is 3";

    ColValue v = list_get(list, 0);
    EXPECT_EQ(v.vlong, 1) << "value at index 0 is 1";
    v = list_get(list, 1);
    EXPECT_EQ(v.vlong, 2) << "value at index 1 is 2";
    v = list_get(list, 2);
    EXPECT_EQ(v.vlong, 3) << "value at index 2 is 3";
}

TEST_F(ArrayListTest, test_insert) {
    for (size_t i = 0; i < 9; ++i) {
        list_append(list, (ColValue){.vlong = static_cast<long>(i), .value_type = COL_TYPE_LONG});
    }
    EXPECT_EQ( list_size(list), 9 ) << "size after adding 9 elements is 9";
    EXPECT_EQ( list_get(list, 4).vlong, 4 ) << "value of index 4 is 4";

    list_insert(list, 4, (ColValue){.vlong = 99, .value_type = COL_TYPE_LONG } );
    EXPECT_EQ( list_get(list, 4).vlong, 99 ) << "value of index 4 is now 99 after list_insert()";

    for (int i = 5; i < 10; ++i) {
        ColValue v = list_get(list, i);
        EXPECT_EQ(v.vlong, i - 1 );
    }
}

TEST_F(ArrayListTest, test_is_empty) {
    EXPECT_TRUE(list_is_empty(list));
}

TEST_F(ArrayListTest, test_remove) {
    for (size_t i = 0; i < 10; ++i) {
        list_append(list, (ColValue){.vlong = static_cast<long>(i), .value_type = COL_TYPE_LONG});
    }
    EXPECT_EQ(list_size(list), 10) << "list size after appending 10 items is 10";

    //remove element index 4
    ColValue v = list_remove(list, 4);
    EXPECT_EQ(v.vlong, 4 ) << "removed value at index 4 is 4";
    EXPECT_EQ( list_size(list), 9 ) << "size after removing 1 element is 9";
    EXPECT_EQ( list_get(list, 4).vlong, 5 ) << "value of index 4 is now 5";

    for (int i = 4; i < 9; ++i) {
        v = list_get(list, i);
        EXPECT_EQ(v.vlong, i+1 );
    }
}

