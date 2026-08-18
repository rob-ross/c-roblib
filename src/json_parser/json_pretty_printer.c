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
#include <stdarg.h>

#include "roblib/string_builder.h"

#ifdef __APPLE__
    // macOS does not provide <uchar.h>, so we define char8_t manually
    typedef unsigned char char8_t;
#else
// Other operating systems supporting C23
#include <uchar.h>
#endif

typedef int (*jsonp_vprint_fn)(void *target, const char *format, va_list args);

typedef struct {
    jsonp_vprint_fn vprint;
    void *target;
} JsonWriter;


typedef struct jsonp_print_context_s {
    JsonWriter writer; // writer interface for printing to a stream or buffer
    int total_chars_printed;
    const JsonFormatFlags flags;
} JsonPrintContext;

constexpr JsonFormatFlags JSON_FORMAT_FLAGS_DEFAULT = { .indent = 2, .single_line = true};

static constexpr size_t MAX_NESTING_DEPTH = 256;
static constexpr char SPACE = ' ';
static constexpr char NUL = '\0';
static constexpr char NEWLINE = '\n';

static char SPACERS[MAX_NESTING_DEPTH][MAX_NESTING_DEPTH] = {};

// -----------------------------------------------------------------
//      Function Prototypes
// -----------------------------------------------------------------

static void jsonp_print_multi_line( const JsonValue *jval,  JsonPrintContext *context, unsigned nesting_level);
static void jsonp_print_single_line( const JsonValue *jval, JsonPrintContext *context, unsigned nesting_level);



static void double_to_str(char *buf, size_t buf_size, double val) {
    // Format to 17 significant digits with minimal trailing zero clutter
    snprintf(buf, buf_size, "%.*g", DBL_DECIMAL_DIG, val);

    // If there is no '.' and no 'e'/'E', append '.0' to force floating-point syntax
    if (strpbrk(buf, ".eE") == nullptr ) {
        strncat(buf, ".0", buf_size - strlen(buf) - 1);
    }
}

static void long_double_to_str(char *buf, size_t buf_size, long double val) {
    // 1. Format using %Lg and LDBL_DECIMAL_DIG precision
    snprintf(buf, buf_size, "%.*Lg", LDBL_DECIMAL_DIG, val);

    // 2. Ensure floating-point indicator is present for the parser
    if (strpbrk(buf, ".eE") == nullptr ) {
        strncat(buf, ".0", buf_size - strlen(buf) - 1);
    }
}



// Implementation for FILE* (like stdout or a disk file)
static int file_vprint(void *target, const char *format, va_list args) {
    return vfprintf((FILE *)target, format, args);
}

// Implementation for your StringBuilder
static int sb_vprint(void *target, const char *format, va_list args) {
    StringBuilder *sb = (StringBuilder *)target;
    // todo (rob) large strings might exceed this buf size.
    // we need to dynamically allocate the buffer on a scratch arena
    // compute the size via snprintf first.
    char buf[1024]; // Temporary buffer for formatting

    int len = vsnprintf(buf, sizeof(buf), format, args);
    if (len > 0) {
        sb_append_str(sb, buf);
    }
    return len;
}

static int mprintf(JsonPrintContext *context, const char *format, ...) {
    va_list args = {};
    va_start(args, format);
    int result = context->writer.vprint(context->writer.target, format, args);
    va_end(args);
    context->total_chars_printed += result;
    return result;
}

static int jsonp_fprint_string( const JsonValue *jval,  JsonPrintContext *context) {
    int chars_printed = 0;
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
    // todo (rob) error checking results of mprintf
    mprintf(context, "\""); // opening quotes
    chars_printed++;
    for (size_t i = 0; i < len; ++i) {
        const unsigned char c = (unsigned char)str[i];
        if (c <= 0x1F) {
            switch (c) {
                case '\b': mprintf(context, "\\b"); chars_printed += 2; break;
                case '\t': mprintf(context, "\\t"); chars_printed += 2; break;
                case '\n': mprintf(context, "\\n"); chars_printed += 2; break;
                case '\f': mprintf(context, "\\f"); chars_printed += 2; break;
                case '\r': mprintf(context, "\\r"); chars_printed += 2; break;
                default:   mprintf(context, "\\u%.4X", c); chars_printed += 6; break;
            }
        } else if ( c == 0x22 || c == 0x5C ) { // quotation mark, reverse solidus (backslash)
            mprintf(context, "\\%c", c);
            chars_printed += 2;
        } else {
            mprintf(context, "%c", c);
            // fputc(c, stream);
            chars_printed++;
        }
    }
    mprintf(context, "\""); // closing quotes
    chars_printed++;
    return chars_printed;
}

void jsonp_fprint_object_single_line( const JsonValue *jval, JsonPrintContext *context,  unsigned nesting_level ) {
    assert (jval->type == JSON_OBJECT);
    const JsonFormatFlags *flags = &context->flags;


    if (jval->u.object.count == 0 ) {
        mprintf(context, "{ }" );
        return;
    }
    mprintf(context, "{ ");

    // First entry
    // key
    mprintf(context, "\"%s\" : ", SLICE_BUF(jval->u.object.entries[0]->key, 128));
    // value
    jsonp_print_single_line(jval->u.object.entries[0]->value, context, nesting_level + 1 );

    for (size_t i = 1; i < jval->u.object.count; ++i) {
        // key
        mprintf(context, ", \"%s\" : ", SLICE_BUF(jval->u.object.entries[i]->key, 128));
        // value
        jsonp_print_single_line(jval->u.object.entries[i]->value, context, nesting_level + 1 );
    }
    mprintf(context, " }");
}

void jsonp_fprint_object_multi_line( const JsonValue *jval, JsonPrintContext *context,  unsigned nesting_level ) {
    assert (jval->type == JSON_OBJECT);
    const JsonFormatFlags *flags = &context->flags;

    char const *indent_str = nullptr;

    if (jval->u.object.count == 0 ) {
        mprintf(context, "}");  // empty object, closing brace
        return;
    }

    nesting_level++;
    indent_str = SPACERS[nesting_level * flags->indent ];
    // we print a newline after the open brace '{' and add spaces for the current indentation amount
    mprintf(context, "\n%s", indent_str );

    // First entry
    // key
    mprintf(context, "\"%s\" : ", SLICE_BUF(jval->u.object.entries[0]->key, 128));
    // value
    jsonp_print_multi_line(jval->u.object.entries[0]->value, context, nesting_level );

    for (size_t i = 1; i < jval->u.object.count; ++i) {
        // key
        //  we print a comma, newline, spacer, string key, and colon
        mprintf(context, ",\n%s\"%s\" : ", indent_str, SLICE_BUF(jval->u.object.entries[i]->key, 128));
        // value
        jsonp_print_multi_line(jval->u.object.entries[i]->value, context, nesting_level );
    }
    nesting_level--;
    indent_str = SPACERS[nesting_level * flags->indent ];
    mprintf(context, "\n%s}", indent_str);
}

static void jsonp_fprint_array_single_line( const JsonValue *jval, JsonPrintContext *context, unsigned nesting_level ) {
    assert (jval->type == JSON_ARRAY);
    int chars_printed = 0;
    const JsonFormatFlags *flags = &context->flags;

    if (jval->u.array.count == 0 ) {
        chars_printed += mprintf(context, "[]");
        context->total_chars_printed += chars_printed;
        return;
    }
    chars_printed += mprintf(context, "[ ");
    jsonp_print_single_line(jval->u.array.elements[0], context, nesting_level );
    for (size_t i = 1; i < jval->u.array.count; ++i) {
        chars_printed += mprintf(context, ", ");
        jsonp_print_single_line(jval->u.array.elements[i], context, nesting_level );
    }
    chars_printed += mprintf(context, " ]");
    context->total_chars_printed += chars_printed;
}


void jsonp_fprint_array_multi_line( const JsonValue *jval, JsonPrintContext *context, unsigned nesting_level ) {
    assert (jval->type == JSON_ARRAY);
    const JsonFormatFlags *flags = &context->flags;

    char const *indent_str = nullptr;

    if (jval->u.array.count == 0 ) {
        mprintf( context, "]");  // empty list, closing bracket
        return;
    }

    nesting_level++;
    indent_str = SPACERS[nesting_level * flags->indent ];
    // we print a newline after the open bracket '[' and add spaces for the current indentation amount
    mprintf(context, "\n%s", indent_str );

    // First element
    jsonp_print_multi_line(jval->u.array.elements[0], context, nesting_level  );

    for (size_t i = 1; i < jval->u.array.count; ++i) {
        mprintf(context, ",\n%s", indent_str);  // comma, newline, spacer
        // next element
        jsonp_print_multi_line(jval->u.array.elements[i], context, nesting_level  );
    }
    nesting_level--;
    indent_str = SPACERS[nesting_level * flags->indent ];
    mprintf(context, "\n%s]", indent_str);
}


static int jsonp_fprint_scalar( const JsonValue *jval, JsonPrintContext *context, unsigned nesting_level ) {
    assert (jval->type != JSON_ARRAY &&  jval->type != JSON_OBJECT);
    int chars_printed = 0;

    switch (jval->type) {
        case JSON_NULL:
            chars_printed = mprintf(context, "null");
            break;
        case JSON_BOOLEAN:
            if (jval->u.boolean == true) return mprintf(context, "true");
            chars_printed = mprintf(context, "false");
            break;
        case JSON_LONG:
            chars_printed = mprintf(context, "%ld", jval->u.n_long);
            break;
        // case JSON_LONG_LONG:
        //     chars_printed = mprintf(context, "%lld",jval->.u.n_long_long);
        //     break;
        case JSON_NUMBER:
        case JSON_DOUBLE: {
            char buf[64] = {};
            // todo we need to print the max number of significant digits available for the number.
            double_to_str(buf, sizeof buf, jval->u.n_double);
            chars_printed = mprintf(context, "%s", buf);
            // chars_printed = mprintf(context, "%.*e", DBL_DECIMAL_DIG - 1, jval->u.n_double);
            break;
        }
        // case JSON_LONG_DOUBLE: {
        //     char buf[128] = {};
        //     long_double_to_str(buf, sizeof buf, jval->u.n_long_double);
        //     chars_printed = mprintf(context, "%s", buf);
        //     // chars_printed = mprintf(context, "%.*Lg", LDBL_DECIMAL_DIG, jval->u.n_long_double);
        //     break;
        // }
        case JSON_STRING:
            chars_printed = jsonp_fprint_string(jval, context);
            break;
        default:
            chars_printed = mprintf(context, "unknown value type: %d", jval->type);
            break;
    }
    return chars_printed;
}

static void pvt_init_spacers(void) {
    for (size_t i = 0; i < MAX_NESTING_DEPTH; ++i) {
        char * ptr = SPACERS[i];
        memset(ptr, SPACE, i);
        ptr[i] = NUL;
    }

}

static void jsonp_print_single_line( const JsonValue *jval, JsonPrintContext *context,  unsigned nesting_level) {
    switch (jval->type) {
        case JSON_OBJECT:
            jsonp_fprint_object_single_line(jval, context, nesting_level );
            break;
        case JSON_ARRAY:
            jsonp_fprint_array_single_line(jval, context, nesting_level );
            break;
        default:
            jsonp_fprint_scalar(jval, context, nesting_level );
            break;
    }
}

static void jsonp_print_multi_line( const JsonValue *jval, JsonPrintContext *context, unsigned nesting_level) {
    char const *indent_str = SPACERS[nesting_level * context->flags.indent];

    switch (jval->type) {
        case JSON_OBJECT:
            mprintf( context, "{");
            jsonp_fprint_object_multi_line(jval, context, nesting_level );
            break;
        case JSON_ARRAY:
            mprintf( context, "[");
            jsonp_fprint_array_multi_line(jval, context, nesting_level );
            break;
        default:
            jsonp_fprint_scalar(jval, context, nesting_level );
            break;
    }
}

// JsonPrintContext encapsulates the output location of the printing operation.
// from `jsonp_sprint`, it will print to a StringBuilder via `sb_vprint`
// from `jsonp_fprint`, it will print to a FILE* stream via `file_vprint`
static int jsonp_print_impl( const JsonValue *jval, JsonPrintContext *context ) {
    // one-time initialization of SPACERS
    if (SPACERS[1][0] != SPACE) {
        pvt_init_spacers();
    }

    if (context->flags.single_line) {
        jsonp_print_single_line(jval, context, 0);
    } else {
        jsonp_print_multi_line(jval, context, 0);
    }
    int chars_printed =  context->total_chars_printed;
    return chars_printed;
}

// todo (rob) implement default arg value for flags. Here should default to single line, indent 2
int jsonp_fprint(FILE* stream, const JsonValue *jval, JsonFormatFlags flags  ) {
    JsonPrintContext context = {
        .writer = { .vprint = file_vprint, .target = stream},
        .flags = flags
    };
    int chars_printed = jsonp_print_impl(jval, &context);
    fflush(stream);
    return chars_printed;
}

// print the JSON value to the StringBuilder argument.
int jsonp_sprint(StringBuilder *sb, const JsonValue *jval, JsonFormatFlags flags) {
    JsonPrintContext context = {
        .writer = { .vprint = sb_vprint, .target = sb},
        .flags = flags
    };
    return jsonp_print_impl(jval, &context);
}

// todo (rob) implement default arg value for flags. Here should default to single line, indent 2
int jsonp_print( const JsonValue *jval, JsonFormatFlags flags) {
    return jsonp_fprint(stdout, jval, flags );
}
