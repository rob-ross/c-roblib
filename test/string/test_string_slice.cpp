//  test_string_slice.cpp
//
//  Created by Rob Ross on 8/4/26.
//
//  Copyright (c) 2026.  All rights reserved.

// simple ad-hoc gtest test cases for string_slice.c

#include "gtest/gtest.h"

#include "roblib/string_slice.h"

// SLIT: slice literal
#define SLIT(S) slice_from_cstring((S))

TEST(StringSlice,  slice_from_cstring) {
    char const * cstring="This is a C string";
    size_t cstring_len = strlen(cstring) ;

    StringSlice slice = slice_from_cstring(cstring);

    EXPECT_EQ(slice.length, cstring_len) << "SliceString length expected to equal the argument string length";
    EXPECT_STREQ(slice.data, cstring ) << "SliceString characters should equal cstring characters";

    StringSlice null_str = slice_from_cstring(nullptr);
    EXPECT_EQ(null_str.length, 0) << "SliceString length for null argument expected to equal 0";
    EXPECT_STREQ(null_str.data, "" ) << "SliceString characters for null argument should be empty slice";

    StringSlice empty_str = slice_from_cstring("");
    EXPECT_EQ(empty_str.length, 0) << "SliceString length expected to equal the argument string length";
    EXPECT_STREQ(empty_str.data, "" ) << "SliceString characters should equal cstring characters";
}

TEST(StringSlice, slice_as_cstring) {
    StringSlice slice = slice_from_cstring("This is a C string");

    char const *cstring = strdup(SLICE_BUF(slice, 1024));
    EXPECT_EQ(strlen(cstring), slice.length) << "C-string length expected to equal the argument SliceString length";
    EXPECT_STREQ(cstring, slice.data) << "C-string characters should equal SliceString characters";
    free( (void*) cstring);

    StringSlice empty_slice = slice_from_cstring("");
    const char * cstr =  strdup(SLICE_BUF(empty_slice, 1024));
    EXPECT_EQ(strlen(cstr), 0 ) << "C-string length for empty slice expected to be 0";
    EXPECT_STREQ(cstr, "") << "C-string characters for empty slice should be the empty string";
    free( (void*)cstr);
}

TEST(StringSlice, slice_compare) {
    StringSlice slice_empty  = slice_empty_slice();
    StringSlice slice_a      = slice_from_cstring("a");
    StringSlice slice_apple  = slice_from_cstring("apple");
    StringSlice slice_banana = slice_from_cstring("banana");

    EXPECT_EQ( slice_compare(slice_a, slice_a), 0) << "a == a";
    EXPECT_LT( slice_compare(slice_a, slice_apple), 0) << "a < apple";
    EXPECT_GT( slice_compare(slice_a, slice_empty), 0) << " a > '' ";

    StringSlice substr_a = slice_substring(slice_banana, 1, 2); // "a"

    EXPECT_EQ( slice_compare( slice_a,  substr_a ), 0) << "a == a";
    EXPECT_EQ( slice_compare( substr_a, slice_a  ), 0) << "a == a";

    EXPECT_GT( slice_compare(slice_apple, slice_a ), 0) << "apple > a";
    EXPECT_LT( slice_compare(slice_empty, slice_a ), 0) << "'' < a" ;
}

#if (0)
TEST(StringSlice, slice_compare_by_case) {
    StringSlice slice_apple        = slice_from_cstring("apple");
    StringSlice slice_banana       = slice_from_cstring("banana");
    StringSlice slice_banana_upper = slice_from_cstring("Banana");

    EXPECT_EQ( slice_compare_by_case(slice_apple, slice_apple, true), 0) << "apple == apple";
    EXPECT_EQ( slice_compare_by_case(slice_apple, slice_apple, false), 0) << "apple == apple";

    EXPECT_NE( slice_compare_by_case(slice_banana, slice_banana_upper, true), 0)  << "banana != Banana";
    EXPECT_EQ( slice_compare_by_case(slice_banana, slice_banana_upper, false), 0) << "banana == Banana";
}
#endif

TEST(StringSlice,  slice_empty_slice) {
    StringSlice empty_slice = slice_empty_slice();
    EXPECT_EQ(empty_slice.length, 0) << "empty slice has length 0";
    EXPECT_STREQ(empty_slice.data, "" ) << "empty slice has no characters";
}

TEST(StringSlice, slice_equal ) {
    char const * fixture = "start middle end";
    StringSlice slice = slice_from_cstring(fixture);
    EXPECT_TRUE( slice_equal(slice, slice_from_cstring(fixture)));
    EXPECT_FALSE( slice_equal(slice, slice_from_cstring("not the same fixture")));

    // tests that slices from different base pointers,
    // but indexed to similar content in each are equal
    StringSlice fixture2 = slice_from_cstring("pre start middle end post");
    fixture2 = slice_substring(fixture2, 4, 20);  // "start middle end"
    EXPECT_TRUE( slice_equal(slice, fixture2));
}

TEST(StringSlice, slice_equal_by_case ) {
    char const * fixture1 = "start middle end";
    char const * fixture2 = "START MIDDLE END";
    StringSlice slice = slice_from_cstring(fixture1);
    EXPECT_TRUE(  slice_equal_by_case(slice, slice_from_cstring(fixture1), true));
    EXPECT_TRUE(  slice_equal_by_case(slice, slice_from_cstring(fixture1), false));
    EXPECT_FALSE( slice_equal_by_case(slice, slice_from_cstring(fixture2), true));
    EXPECT_TRUE(  slice_equal_by_case(slice, slice_from_cstring(fixture2), false));

    EXPECT_FALSE( slice_equal_by_case(slice, slice_from_cstring("not the same fixture"), true));
    EXPECT_FALSE( slice_equal_by_case(slice, slice_from_cstring("not the same fixture"), false));

    StringSlice fixture3 = slice_from_cstring("pre start middle end post");
    StringSlice fixture4 = slice_from_cstring("PRE START MIDDLE END POST");

    StringSlice slice3   = slice_substring(fixture3, 4, 20);  // "start middle end"
    StringSlice slice4   = slice_substring(fixture4, 4, 20);  // "START MIDDLE END"

    EXPECT_TRUE(  slice_equal_by_case(slice, slice3, true));
    EXPECT_TRUE(  slice_equal_by_case(slice, slice3, false));
    EXPECT_FALSE( slice_equal_by_case(slice, slice4, true));
    EXPECT_TRUE(  slice_equal_by_case(slice, slice4, false));

    EXPECT_FALSE( slice_equal_by_case(slice, slice_from_cstring("not the same fixture"), true));
    EXPECT_FALSE( slice_equal_by_case(slice, slice_from_cstring("not the same fixture"), false));
}

TEST(StringSlice, slice_take ) {
    const char * slice_chars = "start middle end";
    StringSlice slice = slice_from_cstring(slice_chars);
    char const * str_fixture = "start";
    size_t str_fixture_len = strlen(str_fixture);

    StringSlice taken = slice_take(slice, 5); // "start"
    EXPECT_EQ(taken.length, str_fixture_len) << "SliceString length expected to equal the argument length";
    EXPECT_TRUE(slice_equal(taken, slice_from_cstring(str_fixture))) << "SliceString characters should equal taken substring characters";

    // Verify that our local copy wasn't changed. Ok, not possible *at the moment* because it's a local var, but...
    // This test protects against future refactoring that might return a StringSlice*
    // from slice_take() or take a StringSlice* argument
    EXPECT_EQ(slice.length, strlen(slice_chars)) << "original SliceString length should not change";
    EXPECT_STREQ(slice.data, slice_chars ) << "original SliceString characters should not change";
}

TEST(StringSlice, slice_drop ) {
    const char * slice_chars = "start middle end";
    StringSlice slice = slice_from_cstring(slice_chars);
    char const * str_fixture = " middle end";
    size_t str_fixture_len = strlen(str_fixture);
    size_t drop_len = strlen(slice_chars) - str_fixture_len;

    StringSlice remaining = slice_drop(slice, 5); // "middle end"

    // printf("drop_len = %zu, remaining='" SV_FMT"'\n", drop_len, SV_FIELDS(remaining));

    EXPECT_EQ(remaining.length, str_fixture_len) << "SliceString length expected to equal the remaining length";
    EXPECT_TRUE(slice_equal(remaining, slice_from_cstring(str_fixture))) << "SliceString characters should equal remaining substring characters";


    // Verify that our local copy wasn't changed. Ok, not possible *at the moment* because it's a local var, but...
    // This test protects against future refactoring that might return a StringSlice*
    // from slice_take() or take a StringSlice* argument
    EXPECT_EQ(slice.length, strlen(slice_chars)) << "original SliceString length should not change";
    EXPECT_STREQ(slice.data, slice_chars ) << "original SliceString characters should not change";
}

TEST(StringSlice, slice_substring ) {
    const char * slice_chars = "start middle end";
    StringSlice slice = slice_from_cstring(slice_chars);
    StringSlice substring = slice_substring(slice, 6, 12);
    // putchar('\''); slice_print(substring); putchar('\'');putchar('\n');
    EXPECT_EQ( substring.length, strlen("middle")) << "SliceString length expected to equal the substring length";
    EXPECT_TRUE(slice_equal(substring, slice_from_cstring("middle"))) << "SliceString characters should equal substring characters";

    StringSlice empty_substring = slice_substring( SLIT(""), 0, 0);
    EXPECT_EQ( empty_substring.length, 0);
    EXPECT_TRUE( slice_equal(empty_substring, slice_empty_slice()));
}

TEST(StringSlice, slice_index_of ) {
    StringSlice slice = slice_from_cstring("tok start middle end tok");
    StringSlice empty = slice_empty_slice();

    EXPECT_EQ( slice_index_of( empty, SLIT("")), 0 ) << "empty string pos in empty string is 0"; // index of empty string in empty string
    EXPECT_EQ( slice_index_of( slice, SLIT("")),  0 ) << "index of empty string in 'start middle end'";

    EXPECT_EQ( slice_index_of( slice, SLIT("tok")), 0 ) << "index of 'tok' in 'tok start middle end tok' is 0";


    EXPECT_EQ( slice_index_of( slice, SLIT("middle")), 10 ) << "index of 'end' in 'tok start middle end tok' is 10";
    EXPECT_EQ( slice_index_of( slice, SLIT("not found")), -1 );

    EXPECT_EQ( slice_index_of( SLIT("abcba"), SLIT("b")), 1 ) << "index of 'b' in 'abcba' is 1";

    EXPECT_EQ( slice_index_of( slice, SLIT("end")), 17 ) << "index of 'end' in 'tok start middle end tok' is 17";
}

TEST(StringSlice, slice_index_of_by_case ) {
    StringSlice slice = slice_from_cstring("start middle end");
    StringSlice empty = slice_empty_slice();

    EXPECT_EQ( slice_index_of_by_case( empty, SLIT(""), false), 0 ) << "empty string pos in empty string is 0"; // index of empty string in empty string
    EXPECT_EQ( slice_index_of_by_case( slice, SLIT(""), false),  0);  // index of empty string in "start middle end"
    EXPECT_EQ( slice_index_of_by_case( slice, SLIT("middle"), false), 6 );
    EXPECT_EQ( slice_index_of_by_case( slice, SLIT("not found"), false), -1 );
    EXPECT_EQ( slice_index_of_by_case( SLIT("abc"), SLIT("c"), false), 2 ) << "index of 'c' in 'abc' is 2";
    EXPECT_EQ( slice_index_of_by_case( slice, SLIT("end"), false), 13 ) << "index of 'end' in 'start middle end' is 13";

    StringSlice upper_slice = slice_from_cstring("START MIDDLE END");

    EXPECT_EQ( slice_index_of_by_case( empty, SLIT(""), true), 0 ) << "empty string pos in empty string is 0"; // index of empty string in empty string
    EXPECT_EQ( slice_index_of_by_case( upper_slice, SLIT(""), true),  0);  // index of empty string in "start middle end"
    EXPECT_EQ( slice_index_of_by_case( upper_slice, SLIT("middle"), true), -1 );
    EXPECT_EQ( slice_index_of_by_case( upper_slice, SLIT("MIDDLE"), true), 6 );

    EXPECT_EQ( slice_index_of_by_case( upper_slice, SLIT("not found"), true), -1 );
    EXPECT_EQ( slice_index_of_by_case( SLIT("abc"), SLIT("c"), true), 2 ) << "index of 'c' in 'abc' is 2";
    EXPECT_EQ( slice_index_of_by_case( upper_slice, SLIT("end"), true), -1 ) << "index of 'end' in 'START MIDDLE END' is -1";
    EXPECT_EQ( slice_index_of_by_case( upper_slice, SLIT("END"), true), 13 ) << "index of 'end' in 'start middle end' is 13";

}




TEST(StringSlice, slice_rindex_of ) {
    StringSlice slice = slice_from_cstring("tok start middle end tok");
    StringSlice empty = slice_empty_slice();

    EXPECT_EQ( slice_rindex_of( empty, SLIT("")), 0 ) << "last empty string pos in empty string is len"; // index of empty string in empty string
    EXPECT_EQ( slice_rindex_of( slice, SLIT("")),  24 )  << "last index of empty string in 'tok start middle end tok' is 24";

    EXPECT_EQ( slice_rindex_of( slice, SLIT("tok")), 21 ) << "rindex of 'tok' in 'tok start middle end tok' is 21";
    EXPECT_EQ( slice_rindex_of( slice, SLIT("middle")), 10 );
    EXPECT_EQ( slice_rindex_of( slice, SLIT("not found")), -1 );

    EXPECT_EQ( slice_rindex_of( SLIT("abcba"), SLIT("b")), 3 ) << "rindex of 'b' in 'abcba' is 3";

    EXPECT_EQ( slice_rindex_of( slice, SLIT("end")), 17 ) << "rindex of 'end' in 'tok start middle end tok' is 13";
}

TEST(StringSlice, slice_rindex_of_by_case ) {
    StringSlice slice = slice_from_cstring("tok start middle end tok");
    StringSlice slice2 = slice_from_cstring("abcba");
    StringSlice empty = slice_empty_slice();

    EXPECT_EQ( slice_rindex_of_by_case( empty, SLIT(""), true),  0 ) << "last empty string index in empty string is 0"; // index of empty string in empty string
    EXPECT_EQ( slice_rindex_of_by_case( empty, SLIT(""), false), 0 ) << "last empty string index in empty string is 0"; // index of empty string in empty string

    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT(""), true),  24 )  << "last index of empty string in 'tok start middle end tok' is 10";
    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT(""), false), 24 )  << "last index of empty string in 'tok start middle end tok' is 24";

    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT("middle"), true), 10 )  << "last index of 'middle' in 'tok start middle end tok' is 10";
    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT("middle"), false), 10 ) << "last index of 'middle' in 'tok start middle end tok' is 10";
    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT("MIDDLE"), true), -1 )  << "last index of 'MIDDLE' in 'tok start middle end tok' is -1";
    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT("MIDDLE"), false), 10 ) << "last index of 'MIDDLE' in 'tok start middle end tok' is 10";

    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT("tok"), true), 21 )   << "last index of 'tok' in 'tok start middle end tok' is 21";
    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT("tok"), false), 21 )  << "last index of 'tok' in 'tok start middle end tok' is 21";
    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT("TOK"), true), -1 )   << "last index of 'TOK' in 'tok start middle end tok' is -1";
    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT("TOK"), false), 21 )  << "last index of 'TOK' in 'tok start middle end tok' is 21";

    EXPECT_EQ( slice_rindex_of_by_case(slice2, SLIT("a"), true),  4 ) << "rindex of 'a' in 'abcba' is 4";
    EXPECT_EQ( slice_rindex_of_by_case(slice2, SLIT("a"), false), 4 ) << "rindex of 'a' in 'abcba' is 4";
    EXPECT_EQ( slice_rindex_of_by_case(slice2, SLIT("b"), true),  3 ) << "rindex of 'b' in 'abcba' is 3";
    EXPECT_EQ( slice_rindex_of_by_case(slice2, SLIT("b"), false), 3 ) << "rindex of 'b' in 'abcba' is 3";
    EXPECT_EQ( slice_rindex_of_by_case(slice2, SLIT("A"), true), -1 ) << "rindex of 'A' in 'abcba' is -1";
    EXPECT_EQ( slice_rindex_of_by_case(slice2, SLIT("A"), false), 4 ) << "rindex of 'A' in 'abcba' is 4";
    EXPECT_EQ( slice_rindex_of_by_case(slice2, SLIT("B"), true), -1 ) << "rindex of 'B' in 'abcba' is -1";
    EXPECT_EQ( slice_rindex_of_by_case(slice2, SLIT("B"), false), 3 ) << "rindex of 'B' in 'abcba' is 3";
}



TEST(StringSlice, slice_starts_with) {
    StringSlice slice = slice_from_cstring("start middle end");
    EXPECT_EQ(slice_starts_with(slice, slice_from_cstring("start")), true) << "'start middle end' starts with 'start'";
    EXPECT_EQ(slice_starts_with(slice, slice_from_cstring("START")), false) << "'start middle end' does not start with 'START'";
    EXPECT_EQ(slice_starts_with(slice, slice_from_cstring("end")), false) << "'start middle end' does not start with 'end'";

    // empty slice
    StringSlice empty_slice = slice_empty_slice();
    EXPECT_EQ(slice_starts_with(empty_slice, slice_from_cstring("")), true) << "empty slice starts with empty string";
    EXPECT_EQ(slice_starts_with(empty_slice, slice_from_cstring("not empty")), false) << "empty slice doesn't start with characters";
    // non-empty slice starts with empty slice?
    EXPECT_EQ(slice_starts_with(slice, slice_from_cstring("")), true) << "'start middle end' starts with empty slice" ;


    // note, at the moment, every slice begins with the empty string/slice.
}

TEST(StringSlice, slice_starts_with_by_case ) {
    StringSlice slice = slice_from_cstring("start middle end");
    StringSlice upper_slice = slice_from_cstring("START MIDDLE END");


    EXPECT_EQ(slice_starts_with_by_case(slice, slice_from_cstring("start"), true), true);
    EXPECT_EQ(slice_starts_with_by_case(slice, slice_from_cstring("start"), false), true);
    EXPECT_EQ(slice_starts_with_by_case(slice, slice_from_cstring("START"), true), false);
    EXPECT_EQ(slice_starts_with_by_case(slice, slice_from_cstring("START"), false), true);

    StringSlice empty_slice = slice_empty_slice();

    EXPECT_EQ(slice_starts_with_by_case(empty_slice, slice_from_cstring(""), true), true);
    EXPECT_EQ(slice_starts_with_by_case(empty_slice, slice_from_cstring(""), false), true);

    EXPECT_EQ(slice_starts_with_by_case(empty_slice, slice_from_cstring("foo"), true), false);
    EXPECT_EQ(slice_starts_with_by_case(empty_slice, slice_from_cstring("foo"), false), false);

    EXPECT_EQ(slice_starts_with_by_case(slice, slice_from_cstring(""), true), true);
    EXPECT_EQ(slice_starts_with_by_case(slice, slice_from_cstring(""), false), true);

}

TEST(StringSlice, slice_ends_with) {
    StringSlice slice = slice_from_cstring("start middle end");
    EXPECT_EQ(slice_ends_with(slice, slice_from_cstring("start")), false);
    EXPECT_EQ(slice_ends_with(slice, slice_from_cstring("end")), true);

    EXPECT_EQ(slice_ends_with_by_case(slice, slice_from_cstring("end"), true), true);
    EXPECT_EQ(slice_ends_with_by_case(slice, slice_from_cstring("end"), false), true);
    EXPECT_EQ(slice_ends_with_by_case(slice, slice_from_cstring("END"), true), false);
    EXPECT_EQ(slice_ends_with_by_case(slice, slice_from_cstring("END"), false), true);

    // empty slice
    StringSlice empty_slice = slice_empty_slice();
    EXPECT_EQ(slice_ends_with(empty_slice, empty_slice), true) << "empty slice ends with empty string";
    EXPECT_EQ(slice_ends_with(empty_slice, slice_from_cstring("not empty")), false) << "empty slice doesn't end with characters";

    EXPECT_EQ(slice_ends_with_by_case(empty_slice, slice_from_cstring(""), true), true);
    EXPECT_EQ(slice_ends_with_by_case(empty_slice, slice_from_cstring(""), false), true);

    EXPECT_EQ(slice_ends_with_by_case(empty_slice, slice_from_cstring("foo"), true), false);
    EXPECT_EQ(slice_ends_with_by_case(empty_slice, slice_from_cstring("foo"), false), false);

    // non-empty slice ends with empty slice?
    EXPECT_EQ(slice_ends_with(slice, slice_from_cstring("")), true);
    EXPECT_EQ(slice_ends_with_by_case(slice, slice_from_cstring(""), true), true);
    EXPECT_EQ(slice_ends_with_by_case(slice, slice_from_cstring(""), false), true);

    // note, at the moment, every slice ends with the empty string/slice.

}
