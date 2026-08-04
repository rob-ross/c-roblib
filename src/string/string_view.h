//  string_view.h
//
//  Created by Rob Ross on 8/1/26.
//
//  Copyright (c) 2026.  All rights reserved.

//

#ifndef C_ROBLIB_STRING_SLICE_H
#define C_ROBLIB_STRING_SLICE_H

#include <stddef.h>
#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif



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
char *              slice_as_cstring(StringSlice s, size_t max_chars, char buf[static max_chars + 1 ]);

bool                slice_starts_with( StringSlice s, StringSlice prefix);
bool                slice_starts_with_by_case( StringSlice s,  StringSlice prefix, bool case_sensitive);
bool                slice_ends_with( StringSlice s, StringSlice suffix);
bool                slice_ends_with_by_case( StringSlice s,  StringSlice suffix, bool case_sensitive);

StringSlice         slice_take(StringSlice s, size_t n);
// get the chars after the nth byte
StringSlice         slice_drop(StringSlice s, size_t n);
// todo (rob) should we support a Python-like Range argument here? Allow `step`? Negative indices?
StringSlice         slice_substring(StringSlice s, size_t start, size_t end);

StringSlice         slice_empty_slice();
bool                slice_equal(StringSlice s1, StringSlice s2);
bool                slice_equal_by_case(StringSlice s1, StringSlice s2, bool case_sensitive);
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

StringSliceArray    slice_split(StringSlice s, char const* delimiter);

StringSlice slice_chop_by_delimiter(StringSlice *s, StringSlice delimiter);
StringSlice slice_chop_by_delimiter_str(StringSlice *s, char const * delimiter);


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
size_t              slice_snprint(StringSlice s, size_t n, char buf[static n + 1]);



#ifdef __cplusplus
}
#endif
#endif //C_ROBLIB_STRING_SLICE_H
