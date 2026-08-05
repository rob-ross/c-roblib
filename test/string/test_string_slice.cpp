//  test_string_slice.cpp
//
//  Created by Rob Ross on 8/4/26.
//
//  Copyright (c) 2026.  All rights reserved.

// simple ad-hoc gtest test cases for string_slice.c

#include "gtest/gtest.h"

#include "roblib/string_slice.h"

TEST(StringSlice,  slice_from_cstring) {
    char const * cstring="This is a C string";
    size_t cstring_len = strlen(cstring) ;

    StringSlice slice = slice_from_cstring(cstring);

    ASSERT_EQ(slice.length, cstring_len) << "SliceString length expected to equal the argument string length";
    ASSERT_STREQ(slice.data, cstring ) << "SliceString characters should equal cstring characters";

    StringSlice null_str = slice_from_cstring(nullptr);
    ASSERT_EQ(null_str.length, 0) << "SliceString length for null argument expected to equal 0";
    ASSERT_STREQ(null_str.data, "" ) << "SliceString characters for null argument should be empty slice";

    StringSlice empty_str = slice_from_cstring("");
    ASSERT_EQ(empty_str.length, 0) << "SliceString length expected to equal the argument string length";
    ASSERT_STREQ(empty_str.data, "" ) << "SliceString characters should equal cstring characters";
}

TEST(StringSlice, slice_as_cstring) {
    StringSlice slice = slice_from_cstring("This is a C string");

    char const *cstring = strdup(SLICE_BUF(slice, 1024));
    ASSERT_EQ(strlen(cstring), slice.length) << "C-string length expected to equal the argument SliceString length";
    ASSERT_STREQ(cstring, slice.data) << "C-string characters should equal SliceString characters";
    free( (void*) cstring);

    StringSlice empty_slice = slice_from_cstring("");
    const char * cstr =  strdup(SLICE_BUF(empty_slice, 1024));
    ASSERT_EQ(strlen(cstr), 0 ) << "C-string length for empty slice expected to be 0";
    ASSERT_STREQ(cstr, "") << "C-string characters for empty slice should be the empty string";
    free( (void*)cstr);
}

TEST(StringSlice,  slice_empty_slice) {
    StringSlice empty_slice = slice_empty_slice();
    ASSERT_EQ(empty_slice.length, 0) << "empty slice has length 0";
    ASSERT_STREQ(empty_slice.data, "" ) << "empty slice has no characters";
}

TEST(StringSlice, slice_equal ) {
    char const * fixture = "start middle end";
    StringSlice slice = slice_from_cstring(fixture);
    ASSERT_TRUE( slice_equal(slice, slice_from_cstring(fixture)));
    ASSERT_FALSE( slice_equal(slice, slice_from_cstring("not the same fixture")));

    StringSlice fixture2 = slice_from_cstring("pre start middle end post");
    fixture2 = slice_substring(fixture2, 4, 20);
    ASSERT_TRUE( slice_equal(slice, fixture2));


}

TEST(StringSlice, slice_equal_by_case ) {

}

TEST(StringSlice, slice_starts_with) {
    StringSlice slice = slice_from_cstring("start middle end");
    ASSERT_EQ(slice_starts_with(slice, slice_from_cstring("start")), true);
    ASSERT_EQ(slice_starts_with(slice, slice_from_cstring("end")), false);

    ASSERT_EQ(slice_starts_with_by_case(slice, slice_from_cstring("start"), true), true);
    ASSERT_EQ(slice_starts_with_by_case(slice, slice_from_cstring("start"), false), true);
    ASSERT_EQ(slice_starts_with_by_case(slice, slice_from_cstring("START"), true), false);
    ASSERT_EQ(slice_starts_with_by_case(slice, slice_from_cstring("START"), false), true);

    // empty slice
    StringSlice empty_slice = slice_empty_slice();
    ASSERT_EQ(slice_starts_with(empty_slice, slice_from_cstring("")), true) << "empty slice starts with empty string";
    ASSERT_EQ(slice_starts_with(empty_slice, slice_from_cstring("not empty")), false) << "empty slice doesn't start with characters";

    ASSERT_EQ(slice_starts_with_by_case(empty_slice, slice_from_cstring(""), true), true);
    ASSERT_EQ(slice_starts_with_by_case(empty_slice, slice_from_cstring(""), false), true);

    ASSERT_EQ(slice_starts_with_by_case(empty_slice, slice_from_cstring("foo"), true), false);
    ASSERT_EQ(slice_starts_with_by_case(empty_slice, slice_from_cstring("foo"), false), false);

    // non-empty slice starts with empty slice?
    ASSERT_EQ(slice_starts_with(slice, slice_from_cstring("")), true);
    ASSERT_EQ(slice_starts_with_by_case(slice, slice_from_cstring(""), true), true);
    ASSERT_EQ(slice_starts_with_by_case(slice, slice_from_cstring(""), false), true);

    // note, at the moment, every slice begins with the empty string/slice.
}

TEST(StringSlice, slice_ends_with) {
    StringSlice slice = slice_from_cstring("start middle end");
    ASSERT_EQ(slice_ends_with(slice, slice_from_cstring("start")), false);
    ASSERT_EQ(slice_ends_with(slice, slice_from_cstring("end")), true);

    ASSERT_EQ(slice_ends_with_by_case(slice, slice_from_cstring("end"), true), true);
    ASSERT_EQ(slice_ends_with_by_case(slice, slice_from_cstring("end"), false), true);
    ASSERT_EQ(slice_ends_with_by_case(slice, slice_from_cstring("END"), true), false);
    ASSERT_EQ(slice_ends_with_by_case(slice, slice_from_cstring("END"), false), true);

    // empty slice
    StringSlice empty_slice = slice_empty_slice();
    ASSERT_EQ(slice_ends_with(empty_slice, empty_slice), true) << "empty slice ends with empty string";
    ASSERT_EQ(slice_ends_with(empty_slice, slice_from_cstring("not empty")), false) << "empty slice doesn't end with characters";

    ASSERT_EQ(slice_ends_with_by_case(empty_slice, slice_from_cstring(""), true), true);
    ASSERT_EQ(slice_ends_with_by_case(empty_slice, slice_from_cstring(""), false), true);

    ASSERT_EQ(slice_ends_with_by_case(empty_slice, slice_from_cstring("foo"), true), false);
    ASSERT_EQ(slice_ends_with_by_case(empty_slice, slice_from_cstring("foo"), false), false);

    // non-empty slice ends with empty slice?
    ASSERT_EQ(slice_ends_with(slice, slice_from_cstring("")), true);
    ASSERT_EQ(slice_ends_with_by_case(slice, slice_from_cstring(""), true), true);
    ASSERT_EQ(slice_ends_with_by_case(slice, slice_from_cstring(""), false), true);

    // note, at the moment, every slice ends with the empty string/slice.

}

TEST(StringSlice, slice_take ) {
    const char * slice_chars = "start middle end";
    StringSlice slice = slice_from_cstring(slice_chars);
    char const * str_fixture = "start";
    size_t str_fixture_len = strlen(str_fixture);

    StringSlice taken = slice_take(slice, 5); // "start"
    ASSERT_EQ(taken.length, str_fixture_len) << "SliceString length expected to equal the argument length";
    ASSERT_TRUE(slice_equal(taken, slice_from_cstring(str_fixture))) << "SliceString characters should equal taken substring characters";

    // Verify that our local copy wasn't changed. Ok, not possible *at the moment* because it's a local var, but...
    // This test protects against future refactoring that might return a StringSlice*
    // from slice_take() or take a StringSlice* argument
    ASSERT_EQ(slice.length, strlen(slice_chars)) << "original SliceString length should not change";
    ASSERT_STREQ(slice.data, slice_chars ) << "original SliceString characters should not change";
}

TEST(StringSlice, slice_drop ) {
    const char * slice_chars = "start middle end";
    StringSlice slice = slice_from_cstring(slice_chars);
    char const * str_fixture = " middle end";
    size_t str_fixture_len = strlen(str_fixture);
    size_t drop_len = strlen(slice_chars) - str_fixture_len;

    StringSlice remaining = slice_drop(slice, 5); // "middle end"

    // printf("drop_len = %zu, remaining='" SV_FMT"'\n", drop_len, SV_FIELDS(remaining));

    ASSERT_EQ(remaining.length, str_fixture_len) << "SliceString length expected to equal the remaining length";
    ASSERT_TRUE(slice_equal(remaining, slice_from_cstring(str_fixture))) << "SliceString characters should equal remaining substring characters";


    // Verify that our local copy wasn't changed. Ok, not possible *at the moment* because it's a local var, but...
    // This test protects against future refactoring that might return a StringSlice*
    // from slice_take() or take a StringSlice* argument
    ASSERT_EQ(slice.length, strlen(slice_chars)) << "original SliceString length should not change";
    ASSERT_STREQ(slice.data, slice_chars ) << "original SliceString characters should not change";
}

TEST(StringSlice, slice_substring ) {
    const char * slice_chars = "start middle end";
    StringSlice slice = slice_from_cstring(slice_chars);
    StringSlice substring = slice_substring(slice, 6, 12);
    // putchar('\''); slice_print(substring); putchar('\'');putchar('\n');
    ASSERT_EQ( substring.length, strlen("middle")) << "SliceString length expected to equal the substring length";
    ASSERT_TRUE(slice_equal(substring, slice_from_cstring("middle"))) << "SliceString characters should equal substring characters";
}


