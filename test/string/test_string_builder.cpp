//  test_string_builder.cpp
//
//  Created by Rob Ross on 8/14/26.
//
//  Copyright (c) 2026.  All rights reserved.

#include <gtest/gtest.h>

#include "roblib/allocator.h"
#include "roblib/string_builder.h"

TEST(StringBuilder, sb_from_cstring) {
    ArenaErrResult aer = alok_arena_create(100);
    ASSERT_FALSE(aer.err) << "alok_arena_create returns err=false on success";
    ASSERT_NE(aer.result, nullptr) << "alok_arena_create returns new AlokArea*";
    AlokArena * arena = aer.result;

    char const *test_str1 = "Test string 1";
    StringBuilder *sb =  sb_from_cstring( test_str1, 16, arena);
    EXPECT_EQ(sb->capacity, 21) << "initial capacity for test string is 12";
    EXPECT_EQ(sb->length, strlen(test_str1)) << "initial length for test string is " << strlen(test_str1);
    EXPECT_EQ(sb->arena, arena) << "StringBuilder should store its arena";
    EXPECT_STREQ(test_str1, sb->buffer);

    alok_arena_destroy(arena);
}

TEST(StringBuilder, sb_new) {
    ArenaErrResult aer = alok_arena_create(100);
    ASSERT_FALSE(aer.err) << "alok_arena_create returns err=false on success";
    ASSERT_NE(aer.result, nullptr) << "alok_arena_create returns new AlokArea*";
    AlokArena * arena = aer.result;

    char const *test_str1 = "Test string 1";
    StringBuilder *sb =  sb_new( 16, arena);
    EXPECT_EQ(sb->capacity, 21) << "initial capacity for requested 16 is 21 ";
    EXPECT_EQ(sb->length, 0) << "initial length for sb_new is 0 " ;
    EXPECT_EQ(sb->arena, arena) << "StringBuilder should store its arena";
    EXPECT_STREQ("", sb->buffer);

    StringBuilder *result = sb_append_str(sb, test_str1);
    EXPECT_EQ(sb->capacity, 21) << "capacity after appending is still 21 ";
    EXPECT_EQ(sb->length, strlen(test_str1)) << "length for sb_new is now " <<strlen(test_str1) ;
    EXPECT_STREQ(test_str1, sb->buffer);

    alok_arena_destroy(arena);
}
