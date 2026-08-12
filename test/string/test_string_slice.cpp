//  test_string_slice.cpp
//
//  Created by Rob Ross on 8/4/26.
//
//  Copyright (c) 2026.  All rights reserved.

// simple ad-hoc gtest test cases for string_slice.c

#include "gtest/gtest.h"
#include "roblib/allocator.h"
#include "roblib/base.h"

#include "roblib/string_slice.h"

// SLIT: slice literal convenience macro
#define SLIT(S) slice_from_cstring((S))
#define NL putchar('\n');

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


TEST(StringSlice, slice_compare_by_case) {
    StringSlice slice_apple        = slice_from_cstring("apple");
    StringSlice slice_banana       = slice_from_cstring("banana");
    StringSlice slice_banana_upper = slice_from_cstring("Banana");

    EXPECT_EQ( slice_compare_by_case(slice_apple, slice_apple, true), 0) << "apple == apple";
    EXPECT_EQ( slice_compare_by_case(slice_apple, slice_apple, false), 0) << "apple == apple";

    EXPECT_EQ( slice_compare_by_case(slice_banana, slice_banana_upper, true), 0)  << "banana != Banana";
    EXPECT_NE( slice_compare_by_case(slice_banana, slice_banana_upper, false), 0) << "banana == Banana";
}


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
    char const * lower_fixture = "start middle end";
    char const * upper_fixture = "START MIDDLE END";
    StringSlice slice = slice_from_cstring(lower_fixture);
    EXPECT_TRUE(  slice_equal_by_case(slice, slice_from_cstring(lower_fixture), true));
    EXPECT_TRUE(  slice_equal_by_case(slice, slice_from_cstring(lower_fixture), false));
    EXPECT_TRUE(  slice_equal_by_case(slice, slice_from_cstring(upper_fixture), true));
    EXPECT_FALSE( slice_equal_by_case(slice, slice_from_cstring(upper_fixture), false));

    EXPECT_FALSE( slice_equal_by_case(slice, slice_from_cstring("not the same fixture"), true));
    EXPECT_FALSE( slice_equal_by_case(slice, slice_from_cstring("not the same fixture"), false));

    StringSlice fixture3 = slice_from_cstring("pre start middle end post");
    StringSlice fixture4 = slice_from_cstring("PRE START MIDDLE END POST");

    StringSlice slice3   = slice_substring(fixture3, 4, 20);  // "start middle end"
    StringSlice slice4   = slice_substring(fixture4, 4, 20);  // "START MIDDLE END"

    EXPECT_TRUE(  slice_equal_by_case(slice, slice3, true));
    EXPECT_TRUE(  slice_equal_by_case(slice, slice3, false));
    EXPECT_TRUE(  slice_equal_by_case(slice, slice4, true));
    EXPECT_FALSE( slice_equal_by_case(slice, slice4, false));

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
    StringSlice slice2 = slice_from_cstring("abcba");
    StringSlice empty = slice_empty_slice();

    EXPECT_EQ( slice_index_of( empty,  SLIT("") ), 0 ) << "empty string index in empty string is 0";

    EXPECT_EQ( slice_index_of( slice,  SLIT("") ), 0 )  << "index of empty string in 'tok start middle end tok' is 0";

    EXPECT_EQ( slice_index_of( slice,  SLIT("not found") ), -1 ) << "substring not found returns -1";

    EXPECT_EQ( slice_index_of( slice,  SLIT("middle") ), 10 ) << "index of 'middle' in 'tok start middle end tok' is 10";
    EXPECT_EQ( slice_index_of( slice,  SLIT("MIDDLE") ), -1 ) << "index of 'MIDDLE' in 'tok start middle end tok' is -1";

    EXPECT_EQ( slice_index_of( slice,  SLIT("tok") ),  0 )  << "index of 'tok' in 'tok start middle end tok' is 0";
    EXPECT_EQ( slice_index_of( slice,  SLIT("TOK") ), -1 ) << "index of 'TOK' in 'tok start middle end tok' is -1";

    EXPECT_EQ( slice_index_of( slice2, SLIT("a") ),  0 ) << "index of 'a' in 'abcba' is 0";
    EXPECT_EQ( slice_index_of( slice2, SLIT("b") ),  1 ) << "index of 'b' in 'abcba' is 1";
    EXPECT_EQ( slice_index_of( slice2, SLIT("A") ), -1 ) << "index of 'A' in 'abcba' is -1";
    EXPECT_EQ( slice_index_of( slice2, SLIT("B") ), -1 ) << "index of 'B' in 'abcba' is -1";
}

TEST(StringSlice, slice_index_of_by_case ) {
    StringSlice slice = slice_from_cstring("tok start middle end tok");
    StringSlice slice2 = slice_from_cstring("abcba");
    StringSlice empty = slice_empty_slice();

    EXPECT_EQ( slice_index_of_by_case( empty,  SLIT(""),          true  ),  0 ) << "empty string index in empty string is 0";
    EXPECT_EQ( slice_index_of_by_case( empty,  SLIT(""),          false ),  0 ) << "empty string index in empty string is 0";

    EXPECT_EQ( slice_index_of_by_case( slice,  SLIT(""),          true  ),  0 ) << "index of empty string in 'tok start middle end tok' is 0";
    EXPECT_EQ( slice_index_of_by_case( slice,  SLIT(""),          false ),  0 ) << "index of empty string in 'tok start middle end tok' is 0";

    EXPECT_EQ( slice_index_of_by_case( slice,  SLIT("not found"), true  ), -1 ) << "substring not found returns -1";
    EXPECT_EQ( slice_index_of_by_case( slice,  SLIT("not found"), false ), -1 ) << "substring not found returns -1";

    EXPECT_EQ( slice_index_of_by_case( slice,  SLIT("middle"),    true  ), 10 ) << "index of 'middle' in 'tok start middle end tok' is 10";
    EXPECT_EQ( slice_index_of_by_case( slice,  SLIT("middle"),    false ), 10 ) << "index of 'middle' in 'tok start middle end tok' is 10";
    EXPECT_EQ( slice_index_of_by_case( slice,  SLIT("MIDDLE"),    true  ), 10 ) << "index of 'MIDDLE' in 'tok start middle end tok' is 10";
    EXPECT_EQ( slice_index_of_by_case( slice,  SLIT("MIDDLE"),    false ), -1 ) << "index of 'MIDDLE' in 'tok start middle end tok' is -1";

    EXPECT_EQ( slice_index_of_by_case( slice,  SLIT("tok"),       true  ),  0 ) << "index of 'tok' in 'tok start middle end tok' is 0";
    EXPECT_EQ( slice_index_of_by_case( slice,  SLIT("tok"),       false ),  0 ) << "index of 'tok' in 'tok start middle end tok' is 0";
    EXPECT_EQ( slice_index_of_by_case( slice,  SLIT("TOK"),       true  ),  0 ) << "index of 'TOK' in 'tok start middle end tok' is 0";
    EXPECT_EQ( slice_index_of_by_case( slice,  SLIT("TOK"),       false ), -1 ) << "index of 'TOK' in 'tok start middle end tok' is -1";

    EXPECT_EQ( slice_index_of_by_case( slice2, SLIT("a"),         true  ),  0 ) << "index of 'a' in 'abcba' is 0";
    EXPECT_EQ( slice_index_of_by_case( slice2, SLIT("a"),         false ),  0 ) << "index of 'a' in 'abcba' is 0";
    EXPECT_EQ( slice_index_of_by_case( slice2, SLIT("b"),         true  ),  1 ) << "index of 'b' in 'abcba' is 1";
    EXPECT_EQ( slice_index_of_by_case( slice2, SLIT("b"),         false ),  1 ) << "index of 'b' in 'abcba' is 1";
    EXPECT_EQ( slice_index_of_by_case( slice2, SLIT("A"),         true  ),  0 )  << "index of 'A' in 'abcba' is 0";
    EXPECT_EQ( slice_index_of_by_case( slice2, SLIT("A"),         false ), -1 ) << "index of 'A' in 'abcba' is -1";
    EXPECT_EQ( slice_index_of_by_case( slice2, SLIT("B"),         true  ),  1 ) << "index of 'B' in 'abcba' is 1";
    EXPECT_EQ( slice_index_of_by_case( slice2, SLIT("B"),         false ), -1 ) << "index of 'B' in 'abcba' is -1";
}

TEST(StringSlice, slice_rindex_of ) {
    StringSlice slice = slice_from_cstring( "tok start middle end tok" );
    StringSlice slice2 = slice_from_cstring( "abcba" );
    StringSlice empty = slice_empty_slice();

    EXPECT_EQ( slice_rindex_of( empty, SLIT("")         ),  0 ) << "last empty string index in empty string is 0"; // index of empty string in empty string
    EXPECT_EQ( slice_rindex_of( slice, SLIT("")         ), 24 ) << "last index of empty string in 'tok start middle end tok' is 24";
    EXPECT_EQ( slice_rindex_of( slice, SLIT("not found")), -1 ) << "substring not found returns -1";
    EXPECT_EQ( slice_rindex_of( slice, SLIT("middle")   ), 10 ) << "last index of 'middle' in 'tok start middle end tok' is 10";
    EXPECT_EQ( slice_rindex_of( slice, SLIT("MIDDLE")   ), -1 ) << "last index of 'MIDDLE' in 'tok start middle end tok' is -1";
    EXPECT_EQ( slice_rindex_of( slice, SLIT("tok")      ), 21 ) << "last index of 'tok' in 'tok start middle end tok' is 21";
    EXPECT_EQ( slice_rindex_of( slice, SLIT("TOK")      ), -1 ) << "last index of 'TOK' in 'tok start middle end tok' is -1";
    EXPECT_EQ( slice_rindex_of(slice2, SLIT("a")        ),  4 ) << "rindex of 'a' in 'abcba' is 4";
    EXPECT_EQ( slice_rindex_of(slice2, SLIT("b")        ),  3 ) << "rindex of 'b' in 'abcba' is 3";
    EXPECT_EQ( slice_rindex_of(slice2, SLIT("A")        ), -1 ) << "rindex of 'A' in 'abcba' is -1";
    EXPECT_EQ( slice_rindex_of(slice2, SLIT("B")        ), -1 ) << "rindex of 'B' in 'abcba' is -1";
}

TEST(StringSlice, slice_rindex_of_by_case ) {
    StringSlice slice = slice_from_cstring( "tok start middle end tok" );
    StringSlice slice2 = slice_from_cstring( "abcba" );
    StringSlice empty = slice_empty_slice();

    EXPECT_EQ( slice_rindex_of_by_case( empty, SLIT(""),          true  ),  0 ) << "last empty string index in empty string is 0"; // index of empty string in empty string
    EXPECT_EQ( slice_rindex_of_by_case( empty, SLIT(""),          false ),  0 ) << "last empty string index in empty string is 0"; // index of empty string in empty string

    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT(""),          true  ), 24 ) << "last index of empty string in 'tok start middle end tok' is 24";
    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT(""),          false ), 24 ) << "last index of empty string in 'tok start middle end tok' is 24";

    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT("not found"), true  ), -1 ) << "substring not found returns -1";
    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT("not found"), false ), -1 ) << "substring not found returns -1";

    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT("middle"),    true  ), 10 ) << "last index of 'middle' in 'tok start middle end tok' is 10";
    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT("middle"),    false ), 10 ) << "last index of 'middle' in 'tok start middle end tok' is 10";
    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT("MIDDLE"),    true  ), 10 ) << "last index of 'MIDDLE' in 'tok start middle end tok' is 10";
    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT("MIDDLE"),    false ), -1 ) << "last index of 'MIDDLE' in 'tok start middle end tok' is -1";

    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT("tok"),       true  ), 21 ) << "last index of 'tok' in 'tok start middle end tok' is 21";
    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT("tok"),       false ), 21 ) << "last index of 'tok' in 'tok start middle end tok' is 21";
    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT("TOK"),       true  ), 21 ) << "last index of 'TOK' in 'tok start middle end tok' is 21";
    EXPECT_EQ( slice_rindex_of_by_case( slice, SLIT("TOK"),       false ), -1 ) << "last index of 'TOK' in 'tok start middle end tok' is -1";

    EXPECT_EQ( slice_rindex_of_by_case(slice2, SLIT("a"),         true  ),  4 ) << "rindex of 'a' in 'abcba' is 4";
    EXPECT_EQ( slice_rindex_of_by_case(slice2, SLIT("a"),         false ),  4 ) << "rindex of 'a' in 'abcba' is 4";
    EXPECT_EQ( slice_rindex_of_by_case(slice2, SLIT("b"),         true  ),  3 ) << "rindex of 'b' in 'abcba' is 3";
    EXPECT_EQ( slice_rindex_of_by_case(slice2, SLIT("b"),         false ),  3 ) << "rindex of 'b' in 'abcba' is 3";
    EXPECT_EQ( slice_rindex_of_by_case(slice2, SLIT("A"),         true  ),  4 ) << "rindex of 'A' in 'abcba' is 4";
    EXPECT_EQ( slice_rindex_of_by_case(slice2, SLIT("A"),         false ), -1 ) << "rindex of 'A' in 'abcba' is -1";
    EXPECT_EQ( slice_rindex_of_by_case(slice2, SLIT("B"),         true  ),  3 ) << "rindex of 'B' in 'abcba' is 3";
    EXPECT_EQ( slice_rindex_of_by_case(slice2, SLIT("B"),         false ), -1 ) << "rindex of 'B' in 'abcba' is -1";
}

TEST(StringSlice, slice_partition) {
    StringSlice slice = SLIT("one, 2, three,4,five");

    StringSlice3Tuple s3 = slice_partition(slice, SLIT(","));
    EXPECT_TRUE(slice_equal( s3._1, SLIT("one") )) << "first is 'one'";
    EXPECT_TRUE(slice_equal( s3._2, SLIT(",") )) << "second is ','";
    EXPECT_TRUE(slice_equal( s3._3, SLIT(" 2, three,4,five") )) << "third is ' 2, three,4,five'";

    s3 = slice_partition(s3._3, SLIT(","));
    EXPECT_TRUE(slice_equal( s3._1, SLIT(" 2") )) << "first is ' 2'";
    EXPECT_TRUE(slice_equal( s3._2, SLIT(",") )) << "second is ','";
    EXPECT_TRUE(slice_equal( s3._3, SLIT(" three,4,five") )) << "third is ' three,4,five'";

    StringSlice empty = slice_empty_slice();
    StringSlice3Tuple s3_empty = slice_partition(empty, SLIT(""));
    // degenerate case
    EXPECT_TRUE(slice_equal( s3_empty._1, SLIT("") )) << "first is ''";
    EXPECT_TRUE(slice_equal( s3_empty._2, SLIT("") )) << "second is ''";
    EXPECT_TRUE(slice_equal( s3_empty._3, SLIT("") )) << "third is ''";

    StringSlice3Tuple s3_2;
    StringSlice slice3 = SLIT("Monty Python's Flying Circus");

    // non-empty slice string, separator is empty string
    s3_2 = slice_partition(slice3, SLIT(""));
    EXPECT_TRUE(slice_equal( s3_2._1, SLIT("") )) << "first is '" << SLICE_BUF(s3_2._1, 64) << "', expected ''";
    EXPECT_TRUE(slice_equal( s3_2._2, SLIT("") )) << "second is '" << SLICE_BUF(s3_2._2, 64) << "', expected ''";
    EXPECT_TRUE(slice_equal( s3_2._3, SLIT("Monty Python's Flying Circus") )) << "third is '" << SLICE_BUF(s3_2._3, 64) << "', expected 'Monty Python's Flying Circus'";

    // non-empty slice string, separator is a space
    s3_2 = slice_partition(slice3, SLIT(" "));
    EXPECT_TRUE(slice_equal( s3_2._1, SLIT("Monty") )) << "first is '" << SLICE_BUF(s3_2._1, 64) << "', expected 'Monty'";
    EXPECT_TRUE(slice_equal( s3_2._2, SLIT(" ") )) << "second is '" << SLICE_BUF(s3_2._2, 64) << "', expected ' '";
    EXPECT_TRUE(slice_equal( s3_2._3, SLIT("Python's Flying Circus") )) << "third is '" << SLICE_BUF(s3_2._3, 64) << "', expected 'Python's Flying Circus'";

    // separator not found
    s3_2 = slice_partition(slice3, SLIT("-"));
    EXPECT_TRUE(slice_equal( s3_2._1, SLIT("Monty Python's Flying Circus") )) << "first is '" << SLICE_BUF(s3_2._1, 64) << "', expected 'Monty Python's Flying Circus'";
    EXPECT_TRUE(slice_equal( s3_2._2, SLIT("") )) << "second is '" << SLICE_BUF(s3_2._2, 64) << "', expected ''";
    EXPECT_TRUE(slice_equal( s3_2._3, SLIT("") )) << "third is '" << SLICE_BUF(s3_2._3, 64) << "', expected ''";
}

// todo (rob) implement
TEST(StringSlice, slice_split) {
    char const * fixture = "one, 2, three,4,five";
    ArenaErrResult aer = alok_arena_create(MB(1), false);
    AlokArena * arena = aer.result;

    StringSlice slice = SLIT(fixture);
    StringSliceArray *slice_array = slice_split(arena, slice, SLIT(","));
    slice_print_slice_array(slice_array);putchar('\n');
    // empty string, empty delimiter
    slice_array = slice_split(arena, SLIT(""), SLIT(""));
    slice_print_slice_array(slice_array);putchar('\n');

    // empty string, non-empty delimiter
    slice_array = slice_split(arena, SLIT(""), SLIT(","));
    slice_print_slice_array(slice_array);putchar('\n');

    // non-empty string, empty delimiter
    slice_array = slice_split(arena, SLIT(fixture), SLIT(""));
    slice_print_slice_array(slice_array);putchar('\n');

    // non-empty string, non-empty but missing delimiter
    slice_array = slice_split(arena, SLIT(fixture), SLIT("|"));
    slice_print_slice_array(slice_array);putchar('\n');

    // non-empty string, non-empty delimiter appearing once
    slice_array = slice_split(arena, SLIT(fixture), SLIT("three"));
    slice_print_slice_array(slice_array);putchar('\n');


}
// todo (rob) implement
TEST(StringSlice, slice_split_by_str){}
// todo (rob) implement
TEST(StringSlice, slice_split_to_out_buffer){}

TEST(StringSlice, slice_chop_by_delimiter) {
    StringSlice fixture;
    StringSlice slice1;
    StringSlice chopped_slice;

    // non-empty delimiter for empty string
    fixture = slice_empty_slice();
    slice1 = fixture;
    chopped_slice = slice_chop_by_delimiter( &slice1, SLIT("-"));
    EXPECT_TRUE(slice_equal(chopped_slice, SLIT("") )) << "returns ''";
    EXPECT_TRUE(slice_equal(slice1, SLIT("") )) << "returns ''";
    // SLICE_EVAL(chopped_slice); NL  SLICE_EVAL(slice1); NL

    // empty delimiter for empty string
    slice1 = fixture;
    chopped_slice = slice_chop_by_delimiter( &slice1, SLIT(""));
    EXPECT_TRUE(slice_equal(chopped_slice, SLIT("") )) << "returns ''";
    EXPECT_TRUE(slice_equal(slice1, SLIT("") )) << "returns ''";
    // SLICE_EVAL(chopped_slice); NL  SLICE_EVAL(slice1); NL

    fixture = SLIT("one, 2, three,4,five");
    slice1 = fixture;
    chopped_slice = slice_chop_by_delimiter( &slice1, SLIT(","));
    EXPECT_TRUE(slice_equal(chopped_slice, SLIT("one") )) << "returns 'one'";
    EXPECT_TRUE(slice_equal(slice1, SLIT(" 2, three,4,five") )) << "returns ' 2, three,4,five'";
    // SLICE_EVAL(chopped_slice); NL  SLICE_EVAL(slice1); NL

    fixture = SLIT("a,b,c,d");
    slice1 = fixture;
    chopped_slice = slice_chop_by_delimiter( &slice1, SLIT(","));
    EXPECT_TRUE(slice_equal(chopped_slice, SLIT("a") )) << "returns 'a'";
    EXPECT_TRUE(slice_equal(slice1, SLIT("b,c,d") )) << "returns 'b,c,d'";
    // SLICE_EVAL(chopped_slice); NL  SLICE_EVAL(slice1); NL


    fixture = SLIT(" a, b, c, d ");
    slice1 = fixture;
    chopped_slice = slice_chop_by_delimiter( &slice1, SLIT(","));
    EXPECT_TRUE(slice_equal(chopped_slice, SLIT(" a") )) << "returns ' a'";
    EXPECT_TRUE(slice_equal(slice1, SLIT(" b, c, d ") )) << "returns ' b, c, d '";
    // SLICE_EVAL(chopped_slice); NL  SLICE_EVAL(slice1); NL

    // test that delimiter is not found in string
    fixture = SLIT(" a, b, c, d ");
    slice1 = fixture;
    chopped_slice = slice_chop_by_delimiter( &slice1, SLIT("-"));
    EXPECT_TRUE(slice_equal(chopped_slice, SLIT(" a, b, c, d ") )) << "returns ' a, b, c, d '";
    EXPECT_TRUE(slice_equal(slice1, SLIT("") )) << "returns ''";
    // SLICE_EVAL(chopped_slice); NL  SLICE_EVAL(slice1); NL

    // delimiter at start of slice
    fixture = SLIT(", a, b, c, d ");
    slice1 = fixture;
    chopped_slice = slice_chop_by_delimiter( &slice1, SLIT(","));
    EXPECT_TRUE(slice_equal(chopped_slice, SLIT("") )) << "returns ''";
    EXPECT_TRUE(slice_equal(slice1, SLIT(" a, b, c, d ") )) << "returns ' a, b, c, d '";
    // SLICE_EVAL(chopped_slice); NL  SLICE_EVAL(slice1); NL


    // multi-char delimiter
    fixture = SLIT("oneSEPtwoSEPthree");
    slice1 = fixture;
    chopped_slice = slice_chop_by_delimiter( &slice1, SLIT("SEP"));
    EXPECT_TRUE(slice_equal(chopped_slice, SLIT("one") )) << "returns 'one'";
    EXPECT_TRUE(slice_equal(slice1, SLIT("twoSEPthree") )) << "returns 'twoSEPthree'";
    // SLICE_EVAL(chopped_slice); NL  SLICE_EVAL(slice1); NL

    // delimiter at start
    fixture = SLIT("SEPoneSEPtwoSEPthree");
    slice1 = fixture;
    chopped_slice = slice_chop_by_delimiter( &slice1, SLIT("SEP"));
    EXPECT_TRUE(slice_equal(chopped_slice, SLIT("") )) << "returns ''";
    EXPECT_TRUE(slice_equal(slice1, SLIT("oneSEPtwoSEPthree") )) << "returns 'oneSEPtwoSEPthree'";
    // SLICE_EVAL(chopped_slice); NL  SLICE_EVAL(slice1); NL

    //separator longer than the string
    fixture = SLIT("ab");
    slice1 = fixture;
    chopped_slice = slice_chop_by_delimiter( &slice1, SLIT("SEP"));
    EXPECT_TRUE(slice_equal(chopped_slice, SLIT("ab") )) << "returns 'ab'";
    EXPECT_TRUE(slice_equal(slice1, SLIT("ab") )) << "returns 'ab'";
    // SLICE_EVAL(chopped_slice); NL  SLICE_EVAL(slice1); NL

    // only separator in string
    fixture = SLIT("SEP");
    slice1 = fixture;
    chopped_slice = slice_chop_by_delimiter( &slice1, SLIT("SEP"));
    EXPECT_TRUE(slice_equal(chopped_slice, SLIT("") )) << "returns ''";
    EXPECT_TRUE(slice_equal(slice1, SLIT("") )) << "returns ''";
    // SLICE_EVAL(chopped_slice); NL  SLICE_EVAL(slice1); NL

}

TEST(StringSlice, slice_chop_by_delimiter_str) {
    StringSlice fixture;
    StringSlice slice1;
    StringSlice chopped_slice;

    // we pass a C-string delimiter to slice_chop_by_delimiter_str
    fixture = SLIT("a,b,c");
    slice1 = fixture;
    chopped_slice = slice_chop_by_delimiter_str( &slice1, ",");
    EXPECT_TRUE(slice_equal(chopped_slice, SLIT("a") )) << "returns 'a'";
    EXPECT_TRUE(slice_equal(slice1, SLIT("b,c") )) << "returns 'b,c'";
    // SLICE_EVAL(chopped_slice); NL  SLICE_EVAL(slice1); NL
}

TEST(StringSlice, slice_chop_by_delimiter_char) {
    StringSlice fixture;
    StringSlice slice1;
    StringSlice chopped_slice;

    // we pass a C-string delimiter to slice_chop_by_delimiter_str
    fixture = SLIT("a,b,c");
    slice1 = fixture;
    chopped_slice = slice_chop_by_delimiter_char( &slice1, ',');
    EXPECT_TRUE(slice_equal(chopped_slice, SLIT("a") )) << "returns 'a'";
    EXPECT_TRUE(slice_equal(slice1, SLIT("b,c") )) << "returns 'b,c'";
    // SLICE_EVAL(chopped_slice); NL  SLICE_EVAL(slice1); NL

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
}

TEST(StringSlice, slice_starts_with_by_case ) {
    StringSlice slice = slice_from_cstring("start middle end");
    StringSlice upper_slice = slice_from_cstring("START MIDDLE END");

    EXPECT_EQ(slice_starts_with_by_case(slice, slice_from_cstring("start"), true), true);
    EXPECT_EQ(slice_starts_with_by_case(slice, slice_from_cstring("start"), false), true);
    EXPECT_EQ(slice_starts_with_by_case(slice, slice_from_cstring("START"), true), true);
    EXPECT_EQ(slice_starts_with_by_case(slice, slice_from_cstring("START"), false), false);

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

    StringSlice empty_slice = slice_empty_slice();

    EXPECT_EQ( slice_ends_with(empty_slice, empty_slice),      true) << "empty slice ends with empty string";
    EXPECT_EQ( slice_ends_with(slice, slice_from_cstring("")), true) << "non-empty slice ends with empty slice";
    EXPECT_EQ( slice_ends_with(empty_slice, slice_from_cstring("not empty")), false) << "empty slice doesn't end with 'not empty'";

    EXPECT_EQ(slice_ends_with(slice, slice_from_cstring("start")), false);
    EXPECT_EQ(slice_ends_with(slice, slice_from_cstring("end")), true);
}

TEST(StringSlice, slice_ends_with_by_case) {
    StringSlice slice = slice_from_cstring("start middle end");
    StringSlice empty_slice = slice_empty_slice();

    EXPECT_EQ(slice_ends_with_by_case(slice, slice_from_cstring("end"), true), true);
    EXPECT_EQ(slice_ends_with_by_case(slice, slice_from_cstring("end"), false), true);
    EXPECT_EQ(slice_ends_with_by_case(slice, slice_from_cstring("END"), true), true);
    EXPECT_EQ(slice_ends_with_by_case(slice, slice_from_cstring("END"), false), false);

    EXPECT_EQ(slice_ends_with_by_case(empty_slice, slice_from_cstring(""), true), true);
    EXPECT_EQ(slice_ends_with_by_case(empty_slice, slice_from_cstring(""), false), true);

    EXPECT_EQ(slice_ends_with_by_case(empty_slice, slice_from_cstring("foo"), true), false);
    EXPECT_EQ(slice_ends_with_by_case(empty_slice, slice_from_cstring("foo"), false), false);

    EXPECT_EQ(slice_ends_with_by_case(slice, slice_from_cstring(""), true), true);
    EXPECT_EQ(slice_ends_with_by_case(slice, slice_from_cstring(""), false), true);
}


TEST(StringSlice, slice_trim) {
    StringSlice slice = slice_from_cstring("     5 Leading spaces. 5 Trailing too!     ");
    StringSlice empty = slice_empty_slice();

    EXPECT_TRUE( slice_equal( slice_trim(empty),     empty));
    EXPECT_TRUE( slice_equal( slice_trim(SLIT(" ")), empty));
    EXPECT_TRUE( slice_equal( slice_trim(SLIT("  ")), empty));

    EXPECT_TRUE( slice_equal( slice_trim(SLIT("NOSPACES")),        SLIT("NOSPACES")));
    EXPECT_TRUE( slice_equal( slice_trim(SLIT("  spaces left")),   SLIT("spaces left")));
    EXPECT_TRUE( slice_equal( slice_trim(SLIT("spaces right   ")), SLIT("spaces right")));
    EXPECT_TRUE( slice_equal( slice_trim(slice), SLIT("5 Leading spaces. 5 Trailing too!")));
}
TEST(StringSlice, slice_trim_left) {
    StringSlice slice = slice_from_cstring("     5 Leading spaces. 5 Trailing too!     ");
    StringSlice empty = slice_empty_slice();

    EXPECT_TRUE( slice_equal( slice_trim_left(empty),     empty)) << "actual: '" << SLICE_BUF(slice_trim_left(empty), 64) << "', expected: ''";
    EXPECT_TRUE( slice_equal( slice_trim_left(SLIT(" ")), empty)) << "actual: '" << SLICE_BUF(slice_trim_left(SLIT(" ")), 64) << "', expected: ''";
    EXPECT_TRUE( slice_equal( slice_trim_left(SLIT("  ")), empty)) << "actual: '" << SLICE_BUF(slice_trim_left(SLIT("  ")), 64) << "', expected: ''";

    EXPECT_TRUE( slice_equal( slice_trim_left(SLIT("NOSPACES")),        SLIT("NOSPACES"))) << "actual: '" << SLICE_BUF(slice_trim_left(empty), 64) << "', expected: 'NOSPACES'";
    EXPECT_TRUE( slice_equal( slice_trim_left(SLIT("  spaces left")),   SLIT("spaces left")));
    EXPECT_TRUE( slice_equal( slice_trim_left(SLIT("spaces right   ")), SLIT("spaces right   ")));
    EXPECT_TRUE( slice_equal( slice_trim_left(slice), SLIT("5 Leading spaces. 5 Trailing too!     ")));
}

TEST(StringSlice, slice_trim_right) {
    StringSlice slice = slice_from_cstring("     5 Leading spaces. 5 Trailing too!     ");
    StringSlice empty = slice_empty_slice();

    EXPECT_TRUE( slice_equal( slice_trim_right(empty),     empty)) << "actual: '" << SLICE_BUF(slice_trim_right(empty), 64) << "', expected: ''";
    EXPECT_TRUE( slice_equal( slice_trim_right(SLIT(" ")), empty));
    EXPECT_TRUE( slice_equal( slice_trim_right(SLIT("  ")), empty));

    EXPECT_TRUE( slice_equal( slice_trim_right(SLIT("NOSPACES")),        SLIT("NOSPACES"))) << "actual: '" << SLICE_BUF(slice_trim_right(SLIT("NOSPACES")), 64) << "', expected: 'NOSPACES'";
    EXPECT_TRUE( slice_equal( slice_trim_right(SLIT("  spaces left")),   SLIT("  spaces left"))) << "actual: '"<< SLICE_BUF(slice_trim_right(SLIT("  spaces left")), 64) << "', expected: '  spaces left'";
    EXPECT_TRUE( slice_equal( slice_trim_right(SLIT("spaces right   ")), SLIT("spaces right")))  << "actual: '"<< SLICE_BUF(slice_trim_right(SLIT("spaces right   ")), 64) << "', expected: 'spaces right'";
    EXPECT_TRUE( slice_equal( slice_trim_right(slice), SLIT("     5 Leading spaces. 5 Trailing too!"))) << "actual: '"<< SLICE_BUF(slice_trim_right(slice), 64) << "', expected: '     5 Leading spaces. 5 Trailing too!'";
}

// todo can we test 'slice_fprint' and 'slice_print'?

// todo (rob) implement
TEST(StringSlice, slice_snprint){}
