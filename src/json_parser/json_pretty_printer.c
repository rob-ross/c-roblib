//  json_pretty_printer.c
//
//  Created by Rob Ross on 8/7/26.
//
//  Copyright (c) 2026.  All rights reserved.

//

#include "roblib/json_parser.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <float.h>

#ifdef __APPLE__
    // macOS does not provide <uchar.h>, so we define char8_t manually
    typedef unsigned char char8_t;
#else
// Other operating systems supporting C23
#include <uchar.h>
#endif


static void double_to_str(char *buf, size_t buf_size, double val) {
    // Format to 17 significant digits with minimal trailing zero clutter
    snprintf(buf, buf_size, "%.*g", DBL_DECIMAL_DIG, val);

    // If there is no '.' and no 'e'/'E', append '.0' to force floating-point syntax
    if (strpbrk(buf, ".eE") == nullptr ) {
        strncat(buf, ".0", buf_size - strlen(buf) - 1);
    }
}

void long_double_to_str(char *buf, size_t buf_size, long double val) {
    // 1. Format using %Lg and LDBL_DECIMAL_DIG precision
    snprintf(buf, buf_size, "%.*Lg", LDBL_DECIMAL_DIG, val);

    // 2. Ensure floating-point indicator is present for the parser
    if (strpbrk(buf, ".eE") == nullptr ) {
        strncat(buf, ".0", buf_size - strlen(buf) - 1);
    }
}

void print_json_string(JsonValue jval) {
    // todo (rob) we have to escape certain characters, like backslash and double quote, and control characters
    /* characters that MUST be escaped:
     * quotation mark, reverse solidus, and the control characters (U+0000 through U+001F).
     * anything < 1F must be escaped. we have some standard escape sequences for tabs and spaces:
     * b : backspace, f: form feed, n: line feed, r: carriage return, t: tab, v: vertical tab
        (0: null , ie \0, a: bell are not part of JSON escapes).
        besides these, we need to escape them as \u00XX.
     */
    StringSlice slice = jval.u.string;
    size_t len = slice.length;
    char const * str = slice.data;
    putchar('"');  // opening quotes
    for (size_t i = 0; i < len; ++i) {
        const unsigned char c = (unsigned char)str[i];
        if (c <= 0x1F) {
            switch (c) {
                case '\b': printf("\\b"); break;
                case '\t': printf("\\t"); break;
                case '\n': printf("\\n"); break;
                case '\f': printf("\\f"); break;
                case '\r': printf("\\r"); break;
                default:   printf("\\u%.4X", c); break;
            }
        } else if ( c == 0x22 || c == 0x5C ) {
            printf("\\%c", c);
        } else {
            putchar(c);
        }
    }
    putchar('"');  // closing quotes
}

void jsonp_pretty_print_object(JsonValue jval) {
    assert (jval.type == JSON_ARRAY);

}
void jsonp_pretty_print_array(JsonValue jval) {
    assert (jval.type == JSON_ARRAY);

}

void jsonp_format_scalar(JsonValue jval) {
    assert (jval.type != JSON_ARRAY &&  jval.type != JSON_OBJECT);

    switch (jval.type) {
        case JSON_NULL:
            printf("null");
            break;
        case JSON_BOOLEAN:
            if (jval.u.boolean == true) printf("true");
            else printf("false");
            break;
        case JSON_LONG:
            printf("%ld", jval.u.n_long);
            break;
        // case JSON_LONG_LONG:
        //     printf("%lld",jval.u.n_long_long);
        //     break;
        case JSON_NUMBER:
        case JSON_DOUBLE: {
            char buf[64] = {};
            // todo we need to print the max number of significant digits available for the number.
            double_to_str(buf, sizeof buf, jval.u.n_double);
            printf("%s", buf);
            // printf("%.*e", DBL_DECIMAL_DIG - 1, jval.u.n_double);
            break;
        }
        // case JSON_LONG_DOUBLE: {
        //     char buf[128] = {};
        //     long_double_to_str(buf, sizeof buf, jval.u.n_long_double);
        //     printf("%s", buf);
        //     // printf("%.*Lg", LDBL_DECIMAL_DIG, jval.u.n_long_double);
        //     break;
        // }
        case JSON_STRING:
            print_json_string(jval);
            break;
        default:
            printf("unknown value type: %d", jval.type);
            break;
    }
    char8_t foo;
}
