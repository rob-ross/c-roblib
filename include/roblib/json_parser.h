// json_parser.h
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/06/02 01:37:49 PDT


/*
 *
JSON-text = ws value ws
These are the six structural characters:
begin-array     = ws %x5B ws  ; '[' left square bracket
begin-object    = ws %x7B ws  ; '{' left curly bracket
end-array       = ws %x5D ws  ; ']' right square bracket
end-object      = ws %x7D ws  ; '}' right curly bracket
name-separator  = ws %x3A ws  ; ':' colon
value-separator = ws %x2C ws  ; ',' comma

ws = *(
        %x20 /  ; Space
        %x09 /  ; Horizontal tab
        %x0A /  ; Line feed or New line
        %x0D )  ; Carriage return


The literal names MUST be lowercase.  No other literal names are allowed.
   value = false / null / true / object / array / number / string
*/
#pragma once

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <regex.h>

#include "arena.h"
#include "error_result.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef enum json_type{
    JSON_NULL,
    JSON_BOOLEAN,

    // We need to be able to distinguish ints from floats when we parse and write values.
    JSON_NUMBER,  // generic JSON number type. Implemented as a double.
    JSON_LONG,
    JSON_DOUBLE,
    JSON_LONG_LONG,
    JSON_LONG_DOUBLE,  // for future use. long double == double on my machine :(

    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type;

typedef struct json_value_s JsonValue;
typedef struct json_object_entry_s JsonObjectEntry;

struct json_value_s {
    json_type type;
    union {
        int boolean;
        union {
            long        n_long;
            double      n_double;
            double      n_number;
            long long   n_long_long;
            long double n_long_double;
        };
        const char *string;
        struct {
            JsonValue **elements;
            size_t count;
        } array;
        struct {
            JsonObjectEntry **entries;
            size_t count;
        } object;
    } u;
};

struct json_object_entry_s {
    char const *key;
    JsonValue *value;
};

/*
// We are testing the use of X-macros below
// if you insert or change order of these enum constants, update error_reporting_tests.txt as well
typedef enum json_error_type_e {
    JSON_ERR_NONE                           = 0,
    JSON_ERR_NULL_TEXT,
    JSON_ERR_EMPTY_TEXT,
    JSON_ERR_UNEXPECTED_TEXT,
    JSON_ERR_UNESCAPED_CONTROL_CHAR,
    JSON_ERR_UNEXPECTED_EOF                 = 5,
    JSON_ERR_UNTERMINATED_ARRAY,
    JSON_ERR_UNTERMINATED_STRING,
    JSON_ERR_UNTERMINATED_OBJECT,
    JSON_ERR_MISSING_COMMA,
    JSON_ERR_MISSING_COLON                  = 10,
    JSON_ERR_INVALID_NUMBER_FORMAT,
    JSON_ERR_INVALID_ESCAPE_SEQUENCE,
    JSON_ERR_INVALID_UNICODE_ESCAPE,
    JSON_ERR_NO_PRECEDING_HIGH_SURROGATE,
    JSON_ERR_NO_FOLLOWING_LOW_SURROGATE     = 15,
    JSON_ERR_RESERVED_FOR_HIGH_SURROGATE,
    JSON_ERR_RESERVED_FOR_LOW_SURROGATE,
    JSON_ERR_CODEPOINT_OUT_OF_RANGE,
    JSON_ERR_INVALID_UTF8_START_BYTE,
    JSON_ERR_INVALID_UTF8_CONTINUATION_BYTE = 20,
    JSON_ERR_OVERLONG_SEQUENCE,
    JSON_ERR_MAX_NESTED_DEPTH_EXCEEDED,
    JSON_ERR_UNEXPECTED_UTF16_ENCODING,
    JSON_ERR_UNEXPECTED_UTF32_ENCODING,
    JSON_ERR_BOM_NOT_ALLOWED                 = 25,
    JSON_ERR_TRAILING_COMMA_NOT_ALLOWED,
    JSON_ERR_OUT_OF_MEMORY,
    JSON_ERR_COUNT
} JsonParseErrType;
*/

/* 1. Define the Master List. JSON_ERR_ get prepended to each name via macro expansion */
// if you insert or change order of these enum constants, update error_reporting_tests.txt as well

#define JSON_ERROR_LIST(X) \
    X(NONE) \
    X(NULL_TEXT) \
    X(EMPTY_TEXT) \
    X(UNEXPECTED_TEXT) \
    X(UNESCAPED_CONTROL_CHAR) \
    X(UNEXPECTED_EOF) \
    X(UNTERMINATED_ARRAY) \
    X(UNTERMINATED_STRING) \
    X(UNTERMINATED_OBJECT) \
    X(MISSING_COMMA) \
    X(MISSING_COLON) \
    X(INVALID_NUMBER_FORMAT) \
    X(INVALID_ESCAPE_SEQUENCE) \
    X(INVALID_UNICODE_ESCAPE) \
    X(NO_PRECEDING_HIGH_SURROGATE) \
    X(NO_FOLLOWING_LOW_SURROGATE) \
    X(RESERVED_FOR_HIGH_SURROGATE) \
    X(RESERVED_FOR_LOW_SURROGATE) \
    X(CODEPOINT_OUT_OF_RANGE) \
    X(INVALID_UTF8_START_BYTE) \
    X(INVALID_UTF8_CONTINUATION_BYTE) \
    X(OVERLONG_SEQUENCE) \
    X(MAX_NESTED_DEPTH_EXCEEDED) \
    X(UNEXPECTED_UTF16_ENCODING) \
    X(UNEXPECTED_UTF32_ENCODING) \
    X(BOM_NOT_ALLOWED) \
    X(TRAILING_COMMA_NOT_ALLOWED) \
    X(MISSING_OBJECT_KEY) \
    X(MISSING_OBJECT_VALUE) \
    X(MISSING_ARRAY_ELEMENT) \
    X(OUT_OF_MEMORY) \
    X(COUNT)

/* 2. Expand the list to create the Enum */
typedef enum json_error_type_e {
#define X(name) JSON_ERR_##name,
    JSON_ERROR_LIST(X)
#undef X
} JsonParseErrType;

constexpr uint32_t ERROR_MSG_BUFFER_SIZE = 1023;

typedef struct json_parse_error_s {
    const char *json;
    enum json_error_type_e err_type;
    uint32_t first_bad_char; // position where parsing failed
    uint32_t line;
    uint32_t column;
    uint32_t parse_start;
    uint32_t parse_end;
    char     message[ERROR_MSG_BUFFER_SIZE + 1];
} JsonParseError;

typedef enum json_config_flag_e : uint64_t {
    JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_ARRAYS,
    JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_OBJECTS,
    JSON_CONFIG_ALLOW_UNICODE_U_ESCAPE, // allows Unicode escapes in form of \UXXXXXX (6 hex digits)
    JSON_CONFIG_ALLOW_HEX_x_ESCAPE, // allows hex byte escapes in form of \xXX (2 hex digits)
    JSON_CONFIG_FAIL_ON_INT_OVERFLOW,  // when not set (default) will try to promote number to double
    JSON_CONFIG_FAIL_ON_FLOAT_OVERFLOW,
    // if set and the number is too small to be represented as a float, rejects the JSON text
    // if not set (the default), an under-flowing double becomes 0.0f.
    JSON_CONFIG_FAIL_ON_FLOAT_UNDERFLOW,
    // if set and the JSON text begins with a BOM, reject the JSON text.
    // if not set (the default) ignores a BOM at the start of the JSON text
    JSON_CONFIG_FAIL_ON_INITIAL_BOM,

    //  if set, replace any invalid UTF-8 sequences with the missing symbol character
    //  if not set (the default), rejects the JSON text as invalid if it contains invalid utf-8 sequences.
    JSON_CONFIG_REPLACE_BAD_UTF8,
    //  if set, replace any invalid utf-16 surrogate sequences with the missing symbol character
    //  if not set (the default), rejects the JSON text as invalid if it contains invalid utf-16 surrogate sequences.
    JSON_CONFIG_REPLACE_BAD_SURROGATES,

} JsonConfigFlag;

typedef uint64_t jp_bitset_t;

typedef struct json_context_s JsonContext; // opaque type

// -----------------------------------------------------------------
//      DEFAULT VALUES
// -----------------------------------------------------------------

constexpr jp_bitset_t JSON_CONFIG_FLAGS_DEFAULT     = 0;
constexpr uint32_t    JSON_DEPTH_MAX_DEFAULT        = 64;
char const * const    JSON_WHITESPACE_CHARS_DEFAULT = " \t\n\r";


// -----------------------------------------------------------------
//      INITIALIZE / DESTROY
// -----------------------------------------------------------------

// Must call at application startup to initialize the parser before first use.
/**
 *  Notes:
 *  init() is intended to be called once before the parser is used
 *  destroy() is intended to be called when done using the parser, before the application terminates.
 *  However, this isn't mandatory. init() and destory() calls can bracket a call to jsonp_parse(). It's just more
 *  efficient to only call init() once.
 *
 *  init() sets the value of global variables used by all calls to API methods from multiple threads.
 *  These globals are the bitset config flags, the max depth of nested structures, and the definition of whitespace characters.
 *  These globals are used to initialize a JsonContext when one is not explicitly provided. They become the default values
 *  used by every new JsonContext. But these variables can be changed on a per-JsonContext basis. Thus, every JsonContext
 *  has its own private versions of these variables. Thus, all calls to API methods are thread-safe.
 *
 *  Even if multiple threads are running API methods and another thread calls jsonp_destroy(), those methods will run
 *  to successful completion as they do not rely on any global state. (hmmm, except for running on Windows, which
 *  requires the c_locale_obj. ...WINDOWS!!! GRRR)
 *
 * @return Error if initialization failed
 *
 * // todo (rob) macro for jsonp_init() to support default argument values
 */
Error jsonp_init();
Error jsonp_init_1(jp_bitset_t config_flags);
Error jsonp_init_2(jp_bitset_t config_flags, uint32_t max_depth);
Error jsonp_init_3(jp_bitset_t config_flags, uint32_t max_depth, char const * whitespace_chars);

// call when done with parsing module, frees up resources acquired in init().
void jsonp_destroy(void);

// -----------------------------------------------------------------
//      PARSING
// -----------------------------------------------------------------

JsonValue *jsonp_parse(const char *json_text, JsonParseError *error, Arena *arena);
JsonValue *jsonp_parse_using_context(const char *json_text, JsonParseError *error, Arena *arena, JsonContext *context );


// version that takes an argument, buffer_size, which is the actual size of the JSON text buffer in bytes.
// this method can report errors where it parsed successfully but did not use up the entire buffer
JsonValue *jsonp_parse_ex(const char *json, JsonParseError *error, Arena *arena, uint32_t buffer_size);


//// ------------------------------------------------------------
////
////    GLOBAL STATE
////
//// ------------------------------------------------------------

jp_bitset_t   jsonp_get_config_bitset();
uint32_t      jsonp_get_max_depth();
char const *  jsonp_get_defined_whitespace_chars();


//// ------------------------------------------------------------
////
////    CONTEXT SPECIFIC METHODS
////
//// ------------------------------------------------------------

// caller must free(context) when done with it.
JsonContext *jsonp_copy_global_context();
// caller must free(context) when done with it.
JsonContext *jsonp_get_empty_context();

// -----------------------------------------------------------------
//      CONFIG FLAGS
// -----------------------------------------------------------------

/**
 *  Sets the JsonConfigFlag flags in a jp_bitset_t and returns them to the caller.
 *  The value returned from this function can be used as
 *  the json_init() method's `config_flags` argument.
 *  Example:
 *     uint64_t my_custom_flags =
 *      jsonp_make_config_flag_bitset( 3, (JsonConfigFlag[3]) {
 *          JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_ARRAYS,
 *          JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_OBJECTS,
 *          JSON_CONFIG_ALLOW_UNICODE_U_ESCAPE
 *     });
 *     Error err = jsonp_init_1(my_custom_flags);
 *     ...
 *
 */
jp_bitset_t jsonp_make_config_flag_bitset( uint32_t flag_count, JsonConfigFlag const flags[]);


jp_bitset_t  jsonp_get_context_config_bitset( JsonContext *context );
void jsonp_set_context_config_bitset( JsonContext *context, jp_bitset_t bitset);

bool jsonp_is_context_config_flag_set( const JsonContext *context, JsonConfigFlag flag);
void jsonp_set_context_config_flag( JsonContext *context, JsonConfigFlag flag);
void jsonp_clear_context_config_flag( JsonContext *context, JsonConfigFlag flag);

// -----------------------------------------------------------------
//      MAX DEPTH
// -----------------------------------------------------------------

/**
 *  Sets the maximum nesting depth allowed in the JSON text. If depth is exceeded, the JSON text is
 *  rejected as invalid.
 *  The default is specified in DEPTH_MAX_DEFAULT
 *  @param max_depth the maximum allowed nesting depth of the JSON text structure.
 */
void jsonp_set_context_max_depth(JsonContext *context, uint32_t max_depth);

// -----------------------------------------------------------------
//      WHITESPACE
// -----------------------------------------------------------------

const char  * jsonp_get_context_whitespace_chars( JsonContext *context);

/**
 * Specifies what the parser considers as white space. Replaces the existing definition.
 * Per RFC 8259, these are the white space characters used by default:
 * ws = *(
 *  %x20 Space
 *  %x09 Horizontal tab
 *  %x0A Line feed or New line
 *  %x0D Carriage return)
 *
 *  The C locale defines what counts as a space (via isspace()) as the above characters, and adds:
 *    form feed (`\\f`),
 *    vertical tab (’\v’)
 *  These are not included by default as white space characters in this parser.
 *
 *  Only supports max 16 chars. Chars after the 16th are ignored.
 *
 *
 * @param context
 * @param whitespace_chars the characters that should be treated as white space characters.
 *
 */
void jsonp_set_context_whitespace_chars( JsonContext *context, const char  *whitespace_chars );


// -----------------------------------------------------------------
//      Decimal Seperator Char
// -----------------------------------------------------------------

char jsonp_get_context_decimal_separator( JsonContext *context );

void jsonp_set_context_decimal_separator( JsonContext *context, char c);

// Searches the entries in the JSON object `json_obj` and returns the entry whose key matches the argument `key`.
// Returns nullptr if there is no entry with this key.
JsonObjectEntry * jsonp_entry_for_key(const JsonValue *json_obj, char const * key) ;

// print a string representation of the JSON graph to the console
void jsonp_print_json_value(JsonValue *value);


/**
 * Returns the string name of the JsonParseErrType constant.
 * @param err_type The error type to convert.
 * @return A constant string literal representing the enum name.
 */
const char *jsonp_parse_error_type_name(JsonParseErrType err_type);

void jsonp_print_parse_error(JsonParseError *err);


void jsonp_value_repr(JsonValue *value);

#ifdef __cplusplus
}
#endif

#endif // JSON_PARSER_H
