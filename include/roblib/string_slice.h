//  string_slice.h
//
//  Created by Rob Ross on 8/1/26.
//
//  Copyright (c) 2026.  All rights reserved.

//

#ifndef C_ROBLIB_STRING_SLICE_H
#define C_ROBLIB_STRING_SLICE_H

#include <stddef.h>
#include <stdio.h>

/**
*  Notes
*  The "Coordinate" Model (Substrings and Slices)
*  When dealing with sequences (substrings, ranges, or slices), it is often more helpful to think of indices
*  as the boundaries or the "gutters" between characters. This is the model used by almost all text editors
*  (the cursor position) and slice-based languages like Python, Swift, or even C++ std::string_view.
*       Indices:  0   1   2   3   4   5
*                 | H | e | l | l | o |
    • The character 'H' occupies the space between index 0 and index 1.
    • The substring "Hell" occupies the space between index 0 and index 4.
    • The empty string "" occupies the space between index 0 and index 0.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

// Helper that creates local function call scope buffer for `slice_as_cstring` to write into
#define SLICE_BUF(S, BUFSZ) slice_as_cstring( (S), BUFSZ - 1, (char [BUFSZ]){ } )
// Helper for formatting a StringSlice when printing
#define SV_FMT "%.*s"
// Helper for specifying length and char array of StringSlice for printing
#define SV_FIELDS(S) (int)(S).length, (S).data

typedef struct string_slice_s {
    char const * data;
    size_t length;

} StringSlice;

typedef struct string_slice_3_tuple_s {
    StringSlice _1;
    StringSlice _2;
    StringSlice _3;
} StringSlice3Tuple;

typedef struct string_slice_array_s {
    size_t size;
    StringSlice elements[]; // FMA
} StringSliceArray;

const StringSlice EMPTY_STRING_SLICE = { .data = "", .length = 0};

StringSlice         slice_from_cstring( char const * cstring );
// Writes at most max_chars of the StringSlice `s` into `buf`, plus the null terminator.
// buf must be large enough to accommodate max_chars + null terminator.
// Returns the argument `buf`
// Consider using this method only as a last resort, or when you must call a library function that requires a C string.
char *              slice_as_cstring(StringSlice s, size_t max_chars, char * buf);

// Return a negative number if s1 < s2, 0 if s1 == s2, and a positive number if s1 > 2
// only works for ASCII characters
int                 slice_compare(StringSlice s1, StringSlice s2);

// todo (rob) need to think about this. I don't think comparing by case is useful.... research this
// context is important. compare is supposed to be consistent with equal and hashcode.
// Useful case that might influence this design: sorting a list of strings where "banana" and "Banana" appear
// next to each other in the list, with "banana" before it. Otherwise, standard sorting using compare
// will put "Banana" at the top of the list with all the other strings that start with a capital letter, and
// thus quite far from the lowercase "banana." I think a 2-level sort is needed, one using case-insensitive
// sorting for a gross sort of the list, then a fine-grained sort using case-sensitive sorting.
// int                 slice_compare_by_case(StringSlice s1, StringSlice s2, bool case_sensitive);

StringSlice         slice_empty_slice();
bool                slice_equal(StringSlice s1, StringSlice s2);
bool                slice_equal_by_case(StringSlice s1, StringSlice s2, bool case_sensitive);


// todo (rob) note, take and drop are just special cases of slice_substring(). Perhaps we could use macros to
// allow default values.
// gets the chars up to the nth byte
StringSlice         slice_take(StringSlice s, size_t n);
// get the chars after the nth byte
StringSlice         slice_drop(StringSlice s, size_t n);
// todo (rob) should we support a Python-like Range argument here? Allow `step`? Negative indices?
StringSlice         slice_substring(StringSlice s, size_t start, size_t end);


// Return the lowest index in the slice where substring sub is found within the slice s[start:end].
//  (Optional arguments start and end are interpreted as in slice notation.)
//  Returns -1 if sub is not found.
ssize_t             slice_index_of( StringSlice s, StringSlice subs);
ssize_t             slice_index_of_by_case( StringSlice s, StringSlice subs, bool case_sensitive );
// Returns the highest index in the string where substring `subs` is found, such that `subs` is contained within s.
// Returns -1 if not found
ssize_t             slice_rindex_of( StringSlice s, StringSlice subs);
ssize_t             slice_rindex_of_by_case( StringSlice s,  StringSlice subs, bool case_sensitive);

//Split the string at the first occurrence of `sep`, and return a 3-tuple containing:
// 1. the part before the separator,
// 2. the separator itself, and
// 3. the part after the separator.
// If the separator is not found, return a 3-tuple containing the string itself, followed by two empty strings.
StringSlice3Tuple   slice_partition(StringSlice s,StringSlice sep);


// split can be implemented several ways. In Python, you can split on a str, a sequence of chars, not just
// a single char. Python returns the results of a split in a list with all the split parts as
// separate strings. There are specific rules, and some of the list elements may be the empty string.
// Another way is to adopt a stateful process where you call split mulitple times. Each time it splits
// the string in half by the first delimiter and returns to you the first string, and modifies
// the argument slice to represent the remainder of the string after the first split.


StringSliceArray slice_split(StringSlice s, StringSlice delimiter);
StringSliceArray slice_split_by_str(StringSlice s, char const * delimiter);

size_t slice_split_to_out_buffer(
            StringSlice s,
            StringSlice delimiter,
            size_t out_count,
            StringSlice *out,
            size_t *consumed) ;


StringSlice slice_chop_by_delimiter(StringSlice *s, StringSlice delimiter);
StringSlice slice_chop_by_delimiter_str(StringSlice *s, char const * delimiter);

bool                slice_starts_with( StringSlice s, StringSlice prefix);
bool                slice_starts_with_by_case( StringSlice s,  StringSlice prefix, bool case_sensitive);
bool                slice_ends_with( StringSlice s, StringSlice suffix);
bool                slice_ends_with_by_case( StringSlice s,  StringSlice suffix, bool case_sensitive);

StringSlice         slice_trim(StringSlice s);
StringSlice         slice_trim_left(StringSlice s);
StringSlice         slice_trim_right(StringSlice s);


// -----------------------------------------------------------------
//      Output Methods
// -----------------------------------------------------------------

// print the value of slice to `stream`
// Returns the number of chars written
size_t              slice_fprint(StringSlice s, FILE* stream );
// print value of slice to stdout
// Returns the number of chars written
size_t              slice_print(StringSlice s);
// Writes the first `n` characters of the slice into the provided buffer, followed by the null terminator.
//
// If the StringSlice has fewer characters than `n`, only s.length characters are written.
// Assumes buf is large enough to accommodate n chars plus the null terminator. I.e., bufsz == n + 1.
//
// Returns the number of characters written to `buf`, not counting the null terminator
size_t              slice_snprint(StringSlice s, size_t n, char * buf);



#ifdef __cplusplus
}
#endif
#endif //C_ROBLIB_STRING_SLICE_H
