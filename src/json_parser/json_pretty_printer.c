//  json_pretty_printer.c
//
//  Created by Rob Ross on 8/7/26.
//
//  Copyright (c) 2026.  All rights reserved.

//

#include "roblib/json_parser.h"
#include "roblib/roblib_types.h"

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

typedef struct jsonp_format_flags_s {
    u8 indent; // number of spaces to indent each nested level
    bool single_line; // true if should format json text as single line, if false, pretty print multiple lines
} JsonFormatFlags;

constexpr JsonFormatFlags JSON_FORMAT_FLAGS_DEFAULT = { .indent = 2, .single_line = true};

static constexpr size_t MAX_NESTING_DEPTH = 256;
static constexpr char SPACE = ' ';
static constexpr char NUL = '\0';
static constexpr char NEWLINE = '\n';

static char SPACERS[MAX_NESTING_DEPTH][MAX_NESTING_DEPTH] = {};

// -----------------------------------------------------------------
//      Function Prototypes
// -----------------------------------------------------------------

static void jsonp_print_json_impl( const JsonValue *jval, const JsonFormatFlags *flags,  unsigned nesting_level);


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

void jsonp_format_string( const JsonValue *jval) {
    // todo (rob) we have to escape certain characters, like backslash and double quote, and control characters
    /* characters that MUST be escaped:
     * quotation mark, reverse solidus, and the control characters (U+0000 through U+001F).
     * anything < 1F must be escaped. we have some standard escape sequences for tabs and spaces:
     * b : backspace, f: form feed, n: line feed, r: carriage return, t: tab, v: vertical tab
        (0: null , ie \0, a: bell are not part of JSON escapes).
        besides these, we need to escape them as \u00XX.
     */
    StringSlice slice = jval->u.string;
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

void jsonp_pretty_print_object( const JsonValue *jval,  const JsonFormatFlags *flags,  unsigned nesting_level ) {
    assert (jval->type == JSON_OBJECT);
    char const *indent_str = SPACERS[nesting_level * flags->indent ];

    if (jval->u.object.count == 0 ) {
        printf("%s{ }", indent_str);
        return;
    }
    if ( flags->single_line ) {
        printf(" { ");
    } else {
        printf("%s{\n", indent_str);
    }
    nesting_level++;
    indent_str = SPACERS[nesting_level * flags->indent ];
    // First entry
    // key
    if ( flags->single_line ) {
        printf("\"%s\" : ", SLICE_BUF(jval->u.object.entries[0]->key, 128));
    } else {
        printf("%s\"%s\" : ", indent_str, SLICE_BUF(jval->u.object.entries[0]->key, 128));
        JsonType next_type = jval->u.object.entries[0]->value->type;
        if ( next_type == JSON_OBJECT || next_type == JSON_ARRAY) {
            printf("\n");
        }
    }
    // value
    jsonp_print_json_impl(jval->u.object.entries[0]->value, flags, nesting_level );
    for (size_t i = 1; i < jval->u.object.count; ++i) {
        // key
        if ( flags->single_line ) {
            printf(", \"%s\" : ", SLICE_BUF(jval->u.object.entries[i]->key, 128));
        } else {
            printf(",\n%s\"%s\" : ", indent_str, SLICE_BUF(jval->u.object.entries[i]->key, 128));
        }
        // value
        JsonType next_type = jval->u.object.entries[i]->value->type;
        if ( !flags->single_line && (next_type == JSON_OBJECT || next_type == JSON_ARRAY ) ) {
            printf("\n");
        }
        jsonp_print_json_impl(jval->u.object.entries[i]->value, flags, nesting_level );
    }
    nesting_level--;
    indent_str = SPACERS[nesting_level * flags->indent ];
    if ( flags->single_line ) {
        printf(" }");
    } else {
        printf("\n%s}", indent_str);
    }
}

void jsonp_pretty_print_array_prev( const JsonValue *jval, const JsonFormatFlags *flags,  unsigned nesting_level ) {
    assert (jval->type == JSON_ARRAY);
    if (jval->u.array.count == 0 ) {
        printf("[ ]");
        return;
    }
    printf("[ ");
    jsonp_print_json_impl(jval->u.array.elements[0], flags, nesting_level );
    for (size_t i = 1; i < jval->u.array.count; ++i) {
        printf(", ");
        jsonp_print_json_impl(jval->u.array.elements[i], flags, nesting_level );
    }
    printf(" ]");
}


void jsonp_pretty_print_array( const JsonValue *jval, const JsonFormatFlags *flags,  unsigned nesting_level ) {
    assert (jval->type == JSON_ARRAY);
    char const *indent_str = SPACERS[nesting_level * flags->indent];
    if (jval->u.array.count == 0 ) {
        printf("%s[ ]", indent_str);
        return;
    }
    if ( flags->single_line ) {
        printf(" [ ");
    } else {
        printf("%s[\n", indent_str);
    }
    nesting_level++;
    indent_str = SPACERS[nesting_level * flags->indent ];
    // First element
    JsonType next_type = jval->u.array.elements[0]->type;
    if ( next_type != JSON_OBJECT && next_type != JSON_ARRAY) {
        printf("%s", indent_str);
    }
    jsonp_print_json_impl(jval->u.array.elements[0], flags, nesting_level  );
    for (size_t i = 1; i < jval->u.array.count; ++i) {
        if ( flags->single_line ) {
            printf(", ");
        } else {
            printf(",\n");
        }
        next_type = jval->u.array.elements[0]->type;
        if ( next_type != JSON_OBJECT && next_type != JSON_ARRAY) {
            printf("%s", indent_str);
        }
        jsonp_print_json_impl(jval->u.array.elements[i], flags, nesting_level  );
    }
    nesting_level--;
    indent_str = SPACERS[nesting_level * flags->indent ];
    if ( flags->single_line ) {
        printf(" ]");
    } else {
        printf("\n%s]", indent_str);
    }
}


void jsonp_format_scalar( const JsonValue *jval, const JsonFormatFlags *flags,  unsigned nesting_level ) {
    assert (jval->type != JSON_ARRAY &&  jval->type != JSON_OBJECT);

    switch (jval->type) {
        case JSON_NULL:
            printf("null");
            break;
        case JSON_BOOLEAN:
            if (jval->u.boolean == true) printf("true");
            else printf("false");
            break;
        case JSON_LONG:
            printf("%ld", jval->u.n_long);
            break;
        // case JSON_LONG_LONG:
        //     printf("%lld",jval->.u.n_long_long);
        //     break;
        case JSON_NUMBER:
        case JSON_DOUBLE: {
            char buf[64] = {};
            // todo we need to print the max number of significant digits available for the number.
            double_to_str(buf, sizeof buf, jval->u.n_double);
            printf("%s", buf);
            // printf("%.*e", DBL_DECIMAL_DIG - 1, jval->u.n_double);
            break;
        }
        // case JSON_LONG_DOUBLE: {
        //     char buf[128] = {};
        //     long_double_to_str(buf, sizeof buf, jval->u.n_long_double);
        //     printf("%s", buf);
        //     // printf("%.*Lg", LDBL_DECIMAL_DIG, jval->u.n_long_double);
        //     break;
        // }
        case JSON_STRING:
            jsonp_format_string(jval);
            break;
        default:
            printf("unknown value type: %d", jval->type);
            break;
    }
    char8_t foo;
}



static void jsonp_print_json_impl( const JsonValue *jval, const JsonFormatFlags *flags,  unsigned nesting_level) {
    switch (jval->type) {
        case JSON_OBJECT:
            jsonp_pretty_print_object(jval, flags, nesting_level );
            break;
        case JSON_ARRAY:
            jsonp_pretty_print_array(jval, flags, nesting_level );
            break;
        default:
            jsonp_format_scalar(jval, flags, nesting_level );
            break;
    }
}

void pvt_init_spacers(void) {
    for (size_t i = 0; i < MAX_NESTING_DEPTH; ++i) {
        char * ptr = SPACERS[i];
        memset(ptr, SPACE, i);
        ptr[i] = NUL;
    }
    // for (size_t i = 0; i < MAX_NESTING_DEPTH; ++i) {
    //     printf("%.3zu: [%s]\n", i, SPACERS[i]);
    // }


}

void jsonp_print_json( const JsonValue *jval) {
    // one-time initialization of SPACERS
    if (SPACERS[1][0] != SPACE) {
        pvt_init_spacers();
    }

    jsonp_print_json_impl(jval, &(JsonFormatFlags){ .single_line = false, .indent = 2 }, 1);
}
