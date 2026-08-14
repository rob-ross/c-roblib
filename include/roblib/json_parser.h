// json_parser.h
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/06/02 01:37:49 PDT

// version: JSONP v0.1.2


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


#include <stdio.h>
#include <stdint.h>

#include "allocator.h"
#include "error_result.h"
#include "roblib_types.h"
#include "string_builder.h"
#include "string_slice.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef enum json_type_e : char{
    JSON_NULL,
    JSON_BOOLEAN,

    // We need to be able to distinguish ints from floats when we parse and write values.
    JSON_NUMBER,  // generic JSON number type. Implemented as a double.
    JSON_LONG,
    JSON_DOUBLE,
    JSON_LONG_LONG,    // for future use. long == long long on my machine. :(
    JSON_LONG_DOUBLE,  // for future use.

    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

typedef struct json_value_s JsonValue;
typedef struct json_object_entry_s JsonObjectEntry;

struct json_value_s {
    union {
        StringSlice string;
        struct {
            JsonValue **elements;
            size_t count;
        } array;
        struct {
            JsonObjectEntry **entries;
            size_t count;
        } object;
        union {
            long        n_long;
            double      n_double;
            double      n_number;
            // long long   n_long_long;
            // long double n_long_double;
        };
        bool boolean;
    } u;
    JsonType type;
};

struct json_object_entry_s {
    StringSlice key;
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
    JSON_ERR_MISSING_OBJECT_KEY),
    JSON_ERR_MISSING_OBJECT_VALUE),
    JSON_ERR_MISSING_ARRAY_ELEMENT),
    JSON_ERR_OUT_OF_MEMORY,                  = 30
    JSON_ERR_FILE_NOT_FOUND,
    JSON_ERR_FILE_ACCESS_ERROR,
    JSON_ERR_FILE_OPEN_FAILED,
    JSON_ERR_EXPECTED_EOF,
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
    X(FILE_NOT_FOUND) \
    X(FILE_ACCESS_ERROR) \
    X(FILE_OPEN_FAILED) \
    X(EXPECTED_EOF) \
    X(COUNT)

/* 2. Expand the list to create the Enum */
typedef enum json_error_type_e : uint32_t {
#define X(name) JSON_ERR_##name,
    JSON_ERROR_LIST(X)
#undef X
} JsonParseErrType;

constexpr uint32_t ERROR_MSG_BUFFER_SIZE = 1023;

typedef struct json_parse_error_s {
    JsonParseErrType err_type;
    uint32_t         first_bad_char; // position where parsing failed
    uint32_t         line;
    uint32_t         column;
    uint32_t         parse_start;
    uint32_t         parse_end;
    // normally 0, but for certain error reporting, indicates how far ahead the look-ahead-buffer is from the current position
    // e.g., lab_offset == 1 means the lab is one byte ahead of the `first_bad_char` member
    uint32_t         lab_offset;  // look-ahead-buffer offset
    char             look_behind_buffer[41]; // up to - 40 chars from current index position
    char             look_ahead_buffer[41];  // up to + 40 chars from current index position

    char             message[ERROR_MSG_BUFFER_SIZE + 1];
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

extern char const * const    JSON_WHITESPACE_CHARS_DEFAULT;


// -----------------------------------------------------------------
//      INITIALIZE / DESTROY
// -----------------------------------------------------------------

/**
 * @brief Initializes the JSON parser's global state. Must be called before first use. Call `jsonp_destroy()` when
 * done with the parser.
 *
 * This function can be called with 0, 1, 2, or 3 arguments, with defaults provided for omitted ones.
 * It sets the default global configuration for all subsequent parsing operations. These defaults can be
 * overridden on a per-parse basis using a `JsonContext`. E.g., see `jsonp_parse_string_using_context()`.
 *
 * You may call `jsonp_init()` once at program startup and call `jsonp_destroy()` once before exiting. Or you may
 * freely call init/destroy between parse calls. Any pending parsing in any thread will run to completion, as
 * every parse call uses a local `JsonContext`. You may not make any API calls after `jsonp_destroy()` has been called
 * until `jsonp_init()` is called again.
 *
 * `jsonp_init()` sets the values of global variables used by all calls to API methods from any thread. These globals
 *  are the bitset config flags, the max depth of nested structures, and the definition of whitespace characters.
 *  The globals are used to initialize a JsonContext when one is not explicitly provided. They become the default
 *  values used by every new JsonContext. These values can be changed on a per-JsonContext basis.
 *  Every JsonContext has its own private versions of these variables. Thus, all calls to API methods are thread-safe.
 *
 * @param config_flags (Optional) A bitset of `JsonConfigFlag` values. Defaults to `JSON_CONFIG_FLAGS_DEFAULT`.
 * @param max_depth (Optional) The maximum allowed nesting depth of JSON objects and arrays. Defaults to `JSON_DEPTH_MAX_DEFAULT`.
 * @param whitespace_chars (Optional) A string of characters to be considered whitespace. Defaults to `JSON_WHITESPACE_CHARS_DEFAULT`.
 *
 * @return An `Error` struct. The `err` field will be `true` if initialization failed.
 */
#define jsonp_init(...) \
    _jsonp_init_SELECT_(__VA_ARGS__ __VA_OPT__(,) _jsonp_init_3, _jsonp_init_2, _jsonp_init_1, _jsonp_init_0 ) (__VA_ARGS__)

// --- Internal Use Only ---
// Helper macros for providing default arguments to jsonp_init().
#define _jsonp_init_0() (jsonp_init)( JSON_CONFIG_FLAGS_DEFAULT, JSON_DEPTH_MAX_DEFAULT, JSON_WHITESPACE_CHARS_DEFAULT)
#define _jsonp_init_1(_1) (jsonp_init)(_1, JSON_DEPTH_MAX_DEFAULT, JSON_WHITESPACE_CHARS_DEFAULT)
#define _jsonp_init_2(_1, _2) (jsonp_init)(_1, _2, JSON_WHITESPACE_CHARS_DEFAULT)
#define _jsonp_init_3(_1, _2, _3) (jsonp_init)(_1, _2, _3)
#define _jsonp_init_SELECT_(_1, _2, _3, NAME, ...) NAME
Error (jsonp_init)(jp_bitset_t config_flags, uint32_t max_depth, char const * whitespace_chars);

// call when done with parsing module, frees up resources acquired in init().
void jsonp_destroy(void);

// -----------------------------------------------------------------
//      PARSING
// -----------------------------------------------------------------

// JsonValue * jsonp_parse(const char *json_text, JsonParseError *error, AlokArena *arena);

JsonValue * jsonp_parse_string(const char *json_text, JsonParseError *error, AlokArena *arena) ;
JsonValue * jsonp_parse_string_using_context(const char *json_text, JsonParseError *error, AlokArena *arena, JsonContext *context );

// version that takes an argument, buffer_size, which is the actual size of the JSON text buffer in bytes.
// this method can report errors where it parsed successfully but did not use up the entire buffer
JsonValue *jsonp_parse_string_ex(const char *json, JsonParseError *error, AlokArena *arena, uint32_t buffer_size);


JsonValue * jsonp_parse_file(const char *json_filename, JsonParseError *error, AlokArena *arena);

/**
 * The user is responsible for passing a FILE* opened in binary mode ("rb").
 * The behavior is unspecified otherwise and will likely fail on Windows.
 */
JsonValue * jsonp_parse_stream( FILE *fp, JsonParseError *error, AlokArena *arena);

// For future use. Not really tested. Not for production.
// How to integrate with security constraints, authorization, API keys, etc?
JsonValue * jsonp_parse_url( const char* url, JsonParseError *error, AlokArena *arena);

//// ------------------------------------------------------------
////
////    GLOBAL STATE
////    Getters only. Values are set in the `jsonp_init()` methods,
///     or on a per-context basis below.
//// ------------------------------------------------------------

jp_bitset_t   jsonp_get_config_bitset();
uint32_t      jsonp_get_max_depth();
char const *  jsonp_get_defined_whitespace_chars();


//// ------------------------------------------------------------
////
////    CONTEXT SPECIFIC METHODS
////
//// ------------------------------------------------------------

// Allocate a new JsonContext initialized with current global values, for use in `jsonp_parse_xxx_using_context` methods.
// Contexts may be reused between parse calls.
// caller must free(context) when done with it.
JsonContext * jsonp_copy_global_context();
/**
 *  Allocate a new empty JsonContext for use in `jsonp_parse_xxx_using_context` methods.
 *  Caller must free(context) when done with it.
 * @return A newly allocated, zero-initialized JsonContext.
 *  See:
 *  `jsonp_set_context_config_bitset`, `jsonp_set_context_config_flag`, `jsonp_set_context_max_depth`,
 *  and `jsonp_set_context_whitespace_chars` to configure this context before use.
 *  Contexts may be reused between parse calls.
 */
JsonContext *jsonp_make_empty_context(void);

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
 *  Sets the maximum nesting depth allowed in the JSON text for the JsonContext.
 *  If depth is exceeded during parsing, the JSON text is rejected as invalid.
 *  The default is specified in DEPTH_MAX_DEFAULT
 *  @param max_depth the maximum allowed nesting depth of the JSON text structure.
 */
void jsonp_set_context_max_depth(JsonContext *context, uint32_t max_depth);
uint32_t jsonp_get_context_max_depth(JsonContext *context);

// -----------------------------------------------------------------
//      WHITESPACE
// -----------------------------------------------------------------


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
 *    form feed (`\f`),
 *    vertical tab (`\v`)
 *  These are not included by default as white space characters in this parser.
 *
 *  Only supports max 8 ASCII chars. Chars after the 8th are ignored.
 *
 *
 * @param context
 * @param whitespace_chars the characters that should be treated as white space characters.
 *
 */
void jsonp_set_context_whitespace_chars( JsonContext *context, const char  *whitespace_chars );
const char  * jsonp_get_context_whitespace_chars( JsonContext *context);


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


// -----------------------------------------------------------------
//      JSON Pretty Printer
// -----------------------------------------------------------------
typedef struct jsonp_format_flags_s {
    u8 indent; // number of spaces to indent each nested level. More than 4 makes the output very wide
    bool single_line; // true if this should format JSON text as single line, if false, print on multiple lines
} JsonFormatFlags;

// Prints the JsonValue in JSON format to stdout
int jsonp_print( const JsonValue *jval, JsonFormatFlags flags);
int jsonp_fprint( FILE* stream, const JsonValue *jval, JsonFormatFlags flags );
int jsonp_sprint( StringBuilder *sb, const JsonValue *jval, JsonFormatFlags flags);

#ifdef __cplusplus
}
#endif

#endif // JSON_PARSER_H
