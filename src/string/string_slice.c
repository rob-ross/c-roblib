//  string_slice.c
//
//  Created by Rob Ross on 8/1/26.
//
//  Copyright (c) 2026.  All rights reserved.

//

#include "string_slice.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>


// -----------------------------------------------------------------
//      Forward References
// -----------------------------------------------------------------

StringSlice slice_take(StringSlice s, size_t n);


StringSlice slice_empty_slice() {
    return (StringSlice){ .data = "", .length = 0 };
}

// get the chars after the nth byte
StringSlice slice_drop(StringSlice s, size_t n) {
    if ( n > s.length) n = s.length;
    return (StringSlice){ .data = s.data + n, .length = s.length - n };
}

bool slice_equal(StringSlice s1, StringSlice s2) {
    if ( s1.length != s2.length ) return false;
    for (size_t i = 0; i < s1.length; ++i) {
        if (s1.data[i] != s2.data[i] ) return false;
    }
    return true;
}

StringSlice slice_from_cstring( char const * cstring ) {
    return (StringSlice){ .data = cstring, .length = strlen(cstring) };
}

void slice_print(StringSlice s) {
    for (size_t i = 0; i < s.length; i++) {
        putchar(s.data[i]);
    }
}

StringSlice slice_split(StringSlice *s, char delimiter) {
    size_t i = 0;
    while (i < s->length && s->data[i] != delimiter) {
        ++i;
    }
    StringSlice result = slice_take(*s, i);
    if ( i < s->length ) {
        *s = slice_drop(*s, i + 1);
    } else {
        *s = slice_drop (*s, i);
    }
    return result;

}

bool slice_starts_with(const StringSlice slice, const StringSlice prefix) {
    if ( prefix.length > slice.length ) return false;
    for (size_t i = 0; i < prefix.length; ++i) {
        if (slice.data[i] != prefix.data[i] ) return false;
    }
    return true;
}

// get first n bytes of the slice
StringSlice slice_take(StringSlice s, size_t n) {
    if (n > s.length) n = s.length;
    return (StringSlice){ .data = s.data, .length = n };
}

StringSlice slice_trim(StringSlice s) {
    char const * start = s.data;
    while (isspace( (unsigned char) *start)) start++;
    char const * end = s.data + s.length - 1;
    while (isspace( (unsigned char) *end)) end--;
    return (StringSlice){ .data = start, .length = end - start + 1 };
}

void test1(void) {
    StringSlice a = slice_from_cstring("ARCH_BTW");
    StringSlice b = slice_from_cstring("ARCH_BTW");
    StringSlice c = slice_from_cstring("BTW");

    printf("is a equal to b? %d", slice_equal(a,b));
    putchar('\n');
    printf("is a equal to c? %d", slice_equal(a,c));
    putchar('\n');
    printf("does it start with ARCH? %d", slice_starts_with(a,slice_from_cstring("ARCH")));
    putchar('\n');
    printf("does it start with BTW? %d", slice_starts_with(a,c));
    putchar('\n');
}

void test2(void) {
    StringSlice a = slice_from_cstring("LEGACY_DISTRO=Arch");
    StringSlice key = slice_take(a, 13);
    StringSlice val = slice_drop(a, 14);
    printf("key = [");
    slice_print(key);
    printf("]\n");
    printf("val = [");
    slice_print(val);
    printf("]\n");
    printf("original = [");
    slice_print(a);
    printf("]\n");
}

void test3(void) {
    StringSlice s = slice_from_cstring("    subscribe btw     ");
    s = slice_trim(s);
    printf("[");
    slice_print(s);
    printf("]\n");
}

int main(int argc, char *argv[]) {
    StringSlice s = slice_from_cstring("arch,gentoo,fedora");
    printf("legacy distros: \n");

    while (s.length > 0 ) {
        slice_print(slice_split(&s, ','));
        putchar('\n');

    }

}

