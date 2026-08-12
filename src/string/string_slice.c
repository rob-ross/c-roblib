//  string_slice.c
//
//  Created by Rob Ross on 8/1/26.
//
//  Copyright (c) 2026.  All rights reserved.

//

#include "roblib/string_slice.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "roblib/allocator.h"
#include "roblib/base.h"

const StringSlice EMPTY_STRING_SLICE = { .length = 0, .data = ""  };

char * slice_as_cstring(StringSlice s, size_t max_chars, char buf[static max_chars + 1 ]) {
    slice_snprint(s, max_chars, buf);
    return buf;
}

int slice_compare(StringSlice s1, StringSlice s2) {
    if ( s1.length < s2.length ) return -1;
    if ( s1.length > s2.length)  return  1;
    for (size_t i = 0; i < s1.length; ++i) {
        int diff = (unsigned char)s1.data[i] - (unsigned char)s2.data[i];
        if ( diff == 0 ) continue;
        return diff;
    }
    return 0;
}


int slice_compare_by_case(StringSlice s1, StringSlice s2, bool ignore_case) {
    if (ignore_case == false) return slice_compare(s1, s2);
    if ( s1.length < s2.length ) return -1;
    if ( s1.length > s2.length)  return  1;
    for (size_t i = 0; i < s1.length; ++i) {
        int diff = toupper((unsigned char)s1.data[i]) - toupper((unsigned char)s2.data[i]);
        if ( diff == 0 ) continue;
        return diff;
    }
    return 0;
}


StringSlice slice_drop(StringSlice s, size_t n) {
    if ( n > s.length) n = s.length;
    StringSlice result =  (StringSlice){ .data = s.data + n, .length = s.length - n };
    return result;
}

StringSlice slice_empty_slice() {
    return EMPTY_STRING_SLICE;
}

bool slice_ends_with(const StringSlice s, const StringSlice suffix) {
    if ( suffix.length > s.length ) return false;
    for (size_t i = 0 ; i < suffix.length; i++ ) {
        if (s.data[s.length - i] != suffix.data[suffix.length - i] ) return false;
    }
    return true;
}

bool slice_ends_with_by_case(const StringSlice s, const StringSlice suffix, bool ignore_case) {
    if ( ignore_case == false ) return slice_ends_with(s, suffix);

    if ( suffix.length > s.length ) return false;

    for (size_t i = 0 ; i < suffix.length; i++ ) {
        if ( toupper(s.data[s.length - i]) != toupper(suffix.data[suffix.length - i]) ) return false;
    }
    return true;
}

// todo (rob) FUTURE SIMD here?
bool slice_equal(const StringSlice s1, const StringSlice s2) {
    if ( s1.length != s2.length ) return false;
    if (s1.data == s2.data) return true; // same count, and data identity
    for (size_t i = 0; i < s1.length; ++i) {
        if (s1.data[i] != s2.data[i] ) return false;
    }
    return true;
}

// todo (rob) FUTURE SIMD here?
bool slice_equal_by_case(const StringSlice s1, const StringSlice s2, bool ignore_case) {
    if ( ignore_case == false ) return slice_equal(s1, s2);
    if ( s1.length != s2.length ) return false;
    if (s1.data == s2.data) return true; // same count, and data identity

    for (size_t i = 0; i < s1.length; ++i) {
        if ( toupper(s1.data[i]) != toupper(s2.data[i]) ) return false;
    }
    return true;
}

StringSlice slice_from_cstring( char const * cstring ) {
    if (!cstring) return EMPTY_STRING_SLICE;
    StringSlice result =  (StringSlice){ .data = cstring, .length = strlen(cstring) };
    return result;
}

size_t slice_fprint(StringSlice s, FILE* stream ) {
    // using fwrite here instead of fputc because _dl on Discord said so.
    return fwrite( s.data, sizeof(unsigned char), s.length, stream );
}

size_t slice_fprint_slice_array(StringSliceArray *slices, FILE* stream) {
    size_t bytes_written = 0;
    bytes_written += fprintf(stream, "(StringSliceArray)[%zu]{ ", slices->size);
    for (size_t i = 0; i < slices->size; ++i) {
        putchar('\'');
        bytes_written += slice_fprint(slices->elements[i], stream);
        bytes_written += fprintf(stream, "', ");
    }
    bytes_written += fprintf(stream, "}");
    return bytes_written;
}

ssize_t slice_index_of(const StringSlice s, const StringSlice subs) {
    if (subs.length > s.length) return -1;
    if (subs.length == 0) return 0;  // every string starts with the empty string

    size_t subs_len = subs.length;
    for (size_t i = 0; i < s.length - subs_len + 1 ; ++i) {
        if (slice_starts_with(slice_drop(s, i), subs)) {
            return i;
        }
    }
    return -1;
}

ssize_t slice_index_of_by_case(const StringSlice s, const StringSlice subs, bool ignore_case ) {
    if ( ignore_case == false ) return slice_index_of(s, subs);

    if (subs.length > s.length) return -1;
    if (subs.length == 0) return 0;  // every string starts with the empty string

    size_t subs_len = subs.length;
    for (size_t i = 0; i < s.length - subs_len + 1; ++i) {
        if (slice_starts_with_by_case(slice_drop(s, i), subs, ignore_case)) {
            return i;
        }
    }
    return -1;
}


ssize_t slice_rindex_of(const StringSlice s, const StringSlice subs) {
    if (subs.length > s.length) return -1;
    if (subs.length == 0) return s.length;  // every string ends with the empty string

    size_t subs_len = subs.length;
    for (size_t i = s.length - subs_len + 1;  i--> 0; ) {
        if (slice_starts_with(slice_drop(s, i), subs)) {
            return i;
        }
    }
    return -1;
}

ssize_t slice_rindex_of_by_case(const StringSlice s, const StringSlice subs, bool ignore_case) {
    if ( ignore_case == false ) return slice_rindex_of(s, subs);

    if (subs.length > s.length) return -1;
    if (subs.length == 0) return s.length;  // every string ends with the empty string

    size_t subs_len = subs.length;
    for (size_t i = s.length - subs_len + 1;  i--> 0; ) {
        if (slice_starts_with_by_case(slice_drop(s, i), subs, ignore_case)) {
            return i;
        }
    }
    return -1;
}


size_t slice_print(StringSlice s) {
    return slice_fprint(s, stdout);
}

//todo (rob) refactor to call a version that takes a stream, and return the bytes written
size_t slice_print_partition(StringSlice3Tuple s3t) {
    printf("(string_slice_3_tuple_s){ ._1 = '"); slice_print(s3t._1);
    printf("', ._2 = '"); slice_print(s3t._2);
    printf("', ._3 = '"); slice_print(s3t._3);
    printf("' }");
    return 0; // todo temp
}

size_t slice_print_slice_array(StringSliceArray *slices) {
    return slice_fprint_slice_array(slices, stdout);
}

StringSlice3Tuple slice_partition(StringSlice s, StringSlice sep) {
    StringSlice3Tuple result = { ._1 = s, ._2 = EMPTY_STRING_SLICE, ._3 = EMPTY_STRING_SLICE};
    if (sep.length > s.length) return result;

    size_t max_i = s.length - sep.length;
    for (size_t i = 0; i < max_i; ++i) {
        if (slice_starts_with(slice_drop(s, i), sep)) {
            result = (StringSlice3Tuple){
                ._1 = slice_take(s, i),
                ._2 = slice_take(slice_drop(s, i), sep.length),
                ._3 = slice_drop(s, i + sep.length )
            };
            return result;
        }
    }
    return result;
}

size_t slice_snprint(StringSlice s, size_t n, char buf[static n + 1]) {
    if (n > s.length) n = s.length;
    memcpy(buf, s.data, n);
    buf[n] = '\0';
    return n;
}


StringSliceArray * slice_split( AlokArena * arena, StringSlice s, StringSlice delimiter) {
    size_t list_count = 0;
    AlokArenaTemp arena_temp = alok_arena_get_scratch(&(AlokArena*){arena}, 1);
    StringSliceLL sll = {};

    while (s.length > 0 ) {
        StringSlice result =  slice_chop_by_delimiter(&s, delimiter);
        StringSliceNode *node = alok_arena_alloc(arena_temp.arena, sizeof(StringSliceNode), nullptr, alignof(StringSliceNode));
        node->next = nullptr;
        node->slice = result;
        SLLQueuePush(sll.first, sll.last, node );
        list_count++;
    }
    // creating return struct in the caller's arena
    StringSliceArray * array_mem = alok_arena_alloc(arena,
        sizeof(StringSliceArray) + list_count * sizeof(StringSlice), nullptr, alignof(StringSlice));

    array_mem->size = list_count;
    StringSliceNode *node = sll.first;
    for (size_t i = 0 ; i < list_count; ++i, node = node->next) {
        array_mem->elements[i] = node->slice;
    }

    alok_arena_release_scratch(&arena_temp);  // clean up temp memory used

    return array_mem;
}


StringSliceArray * slice_split_by_str( AlokArena * arena, StringSlice s, char const * delimiter) {
    return slice_split(arena, s, slice_from_cstring(delimiter));
}

size_t slice_split_to_out_buffer(
            StringSlice s,
            StringSlice delimiter,
            size_t out_count,
            StringSlice *out,
            size_t *consumed) {

    if (s.length == 0 || out_count == 0 || delimiter.length == 0 ) {
        if (out) {
            out->length = s.length;
            out->data   = s.data;
        }
        return 0;
    }

    size_t result = 1;

    out->length = s.length;
    out->data   = s.data;

    while (s.length > 0 && out_count > 0 ) {
        if (slice_starts_with(s, delimiter)) {
            out_count -= 1;
            if (out_count > 0 ) {
                s.data += delimiter.length;
                s.length -= delimiter.length;

                out += 1;
                out->data = s.data;
                out->length = 0;

                result +=1;
            }

            if (consumed) {
                *consumed += delimiter.length;
            }
        } else {
            s.data   += 1;
            s.length -= 1;
            out->length += 1;

            if (consumed) {
                *consumed +=1;
            }
        }
    }

    return result;
}

// Returns the first substring of s that is followed by the delimiter.
// `s` is modified to contain the remainder of the string following the first delimiter, not including the
// delimiter string.
StringSlice slice_chop_by_delimiter(StringSlice *s, StringSlice delimiter) {
    // note: delimiter may be a temporary buffer created in `slice_chop_by_delimiter_char` as a convenience.
    // if in the future this function needs to return or store the delimiter, it may have a dangling pointer.
    // in this case we either need to make a copy of the delimiter data via strdup(), or
    // remove the function `slice_chop_by_delimiter_char`

    if (delimiter.length == 0 || s->length == 0 || delimiter.length > s->length ) {
        StringSlice result = *s;
        *s = slice_empty_slice();
        return result;
    }
    // special case, s only contains the separator. return empty strings
    if (delimiter.length == s->length && slice_equal(*s, delimiter)) {
        *s = slice_empty_slice();
        return *s;
    }
    const size_t max_i = s->length - delimiter.length;
    size_t i = 0;
    for ( i = 0; i < max_i; ++i) {
        if (slice_starts_with(slice_drop(*s, i), delimiter)) {
            // matched delimiter
            StringSlice result = slice_take(*s, i);
            *s = slice_drop(*s, i + delimiter.length );
            return result;
        }
    }
    // todo (rob) we return the original slice if no delimiter was found. What about the argument slice?
    // do we leave it alone or set it to the empty string? For now I return the original string
    // and modify the argument to set it to the empty slice.
    StringSlice result = *s;
    *s = slice_empty_slice();
    return result;
}

// Convenience method that converts a single `delimiter_char` to a StringSlice then calls
// slice_chop_by_delimiter()
StringSlice slice_chop_by_delimiter_char(StringSlice *s, char const delimiter_char) {
    // note that the temp string only exists during evaluation of this function.
    // if `slice_chop_by_delimiter` retains or returns this delimiter, that StringSlice's .data member
    // is a dangling pointer
    StringSlice result = slice_chop_by_delimiter(s,
        slice_from_cstring(  (char const[2]){delimiter_char, 0} ));
    return result;
}

// Convenience method that converts `delimiter` to a StringSlice then calls
// slice_chop_by_delimiter()
StringSlice slice_chop_by_delimiter_str(StringSlice *s, char const* delimiter) {
    StringSlice result = slice_chop_by_delimiter(s, slice_from_cstring(delimiter));
    return result;
}

bool slice_starts_with(const StringSlice s, const StringSlice prefix) {
    if ( prefix.length > s.length ) return false;
    for (size_t i = 0; i < prefix.length; ++i) {
        if (s.data[i] != prefix.data[i] ) return false;
    }
    return true;
}

bool slice_starts_with_by_case(const StringSlice s, const StringSlice prefix, bool ignore_case) {
    if ( ignore_case == false) return slice_starts_with(s, prefix);
    for (size_t i = 0; i < prefix.length; ++i) {
        if ( toupper(s.data[i]) != toupper(prefix.data[i]) ) return false;
    }
    return true;
}

StringSlice slice_substring(StringSlice s, size_t start, size_t end) {
    if (start > s.length) return EMPTY_STRING_SLICE;
    if ( end > s.length ) end = s.length;
    if (start >= end) return EMPTY_STRING_SLICE;
    StringSlice result = (StringSlice){ .data = s.data + start, .length =  end - start };
    return result;
}
// get first n bytes of the slice
StringSlice slice_take(StringSlice s, size_t n) {
    if (n > s.length) n = s.length;
    StringSlice result = (StringSlice){ .data = s.data, .length = n };
    return result;
}

StringSlice slice_trim(StringSlice s) {
    size_t index = 0;
    size_t start = 0;
    size_t end   = s.length - 1;
    while ( index < s.length && isspace( (unsigned char) s.data[index] )) index++;
    start = index;

    index = s.length;
    while ( index--> start && isspace( (unsigned char) s.data[index] )){};
    end = index;

    StringSlice result = (StringSlice){ .data = s.data + start, .length = end - start + 1 };
    return result;
}

StringSlice slice_trim_left(StringSlice s) {
    size_t index = 0;
    while ( index < s.length && isspace( (unsigned char) s.data[index] )) index++;
    StringSlice result = (StringSlice){ .data = s.data + index, .length = s.length - index   };
    return result;
}
StringSlice slice_trim_right(StringSlice s) {
    size_t index = s.length;
    while ( index--> 0 && isspace( (unsigned char) s.data[index] )){};
    StringSlice result = (StringSlice){ .data = s.data, .length = index + 1   };
    return result;
}


//// ------------------------------------------------------------
////
////    TESTING
////
//// ------------------------------------------------------------



void test_split(void) {
    StringSlice s = slice_from_cstring("arch,gentoo,fedora");
    printf("legacy distros: \n");

    while (s.length > 0 ) {
        slice_print(slice_chop_by_delimiter_char(&s, ','));
        putchar('\n');

    }
}




void examples_of_printing_slice(void) {
    StringSlice s = slice_from_cstring("This is a string Joey.");
    StringSlice substr = slice_substring(s, 10, 15); // "string"
    printf("substring(10,15) is '"); slice_print(substr); printf("'\n");

    // requires a temporary buffer created with function parameter scope.
    printf("printing via slice_as_cstring: `%s`\n", slice_as_cstring(substr, 1000, (char [1000]){ } ));
    printf("printing via slice_as_cstring: `%s`\n", SLICE_BUF(substr, 1024) );
    // note: the SV_FIELDS macro doesn't require a buffer or a copy, so it seems to be the best method of printing.
    printf("printing with SV_FMT: |"SV_FMT"\n", SV_FIELDS(substr));
}
// #define STRING_SLICE_MAIN
#ifdef STRING_SLICE_MAIN
int main(int argc, char *argv[]) {
#if (0)
    test_split();

#endif

}
#endif
