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


A JSON value MUST be an object, array, number, or string, or one of the following three literal names:
   false
   null
   true

The literal names MUST be lowercase.  No other literal names are allowed.
   value = false / null / true / object / array / number / string

false = %x66.61.6c.73.65    ; false
null  = %x6e.75.6c.6c       ; null
true  = %x74.72.75.65       ; true


*/
#pragma once

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <regex.h>

#include "arena.h"
#include "error_result.h"
#include "bitset.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Token {
    TOK_TRUE,
    TOK_FALSE,
    TOK_NULL,
    TOK_LEFT_BRACKET,
    TOK_RIGHT_BRACKET,
    TOK_LEFT_BRACE,
    TOK_RIGHT_BRACE,
    TOK_COLON,
    TOK_COMMA,
    // We are lexing and parsing at the same time in this parser, so these "tokens" really represent a type of AST node
    TOK_STRING, // this is actually a value, not a token... but so are true, false, and null. hmmm...???
    TOK_NUMBER, // also not a token, a value.

};

typedef struct {
    const char *pattern_string;
    regex_t compiled_regex;
    const char *name; // For easier identification in output
    enum Token token;
} RegexPattern;


typedef enum json_type{
    JSON_NULL,
    JSON_BOOLEAN,

    // We need to be able to distinguish ints from floats when we parse and write values.
    JSON_NUMBER,
    JSON_INT,
    JSON_FLOAT,

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
            long   n_long;
            double n_double;
            double n_number;
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

// if you insert or change order of these enum constants, update error_reporting_tests.txt as well
typedef enum json_error_type {
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
} JsonErrType;

constexpr uint32_t ERROR_MSG_BUFFER_SIZE = 1023;

typedef struct json_error_s {
    const char *json;
    enum json_error_type err_type;
    uint32_t first_bad_char; // position where parsing failed
    uint32_t line;
    uint32_t column;
    uint32_t parse_start;
    uint32_t parse_end;
    char     message[ERROR_MSG_BUFFER_SIZE + 1];
} JsonError;

typedef enum json_config_flag_e : uint64_t {
    JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_ARRAYS,
    JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_OBJECTS,
    JSON_CONFIG_ALLOW_UNICODE_U_ESCAPE, // allows Unicode escapes in form of \UXXXXXX (6 hex digits)
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

BITSET_DEFINE(JsonConfigFlagSet, 64);

// -----------------------------------------------------------------
//      DEFAULT VALUES
// -----------------------------------------------------------------

constexpr uint64_t JSON_CONFIG_FLAGS_DEFAULT     = 0;
constexpr uint32_t JSON_DEPTH_MAX_DEFAULT        = 64;
char const * const JSON_WHITESPACE_CHARS_DEFAULT = " \t\n\r";


// -----------------------------------------------------------------
//      INITIALIZE / DESTROY
// -----------------------------------------------------------------

// Must call at application startup to initialize the parser before first use.
Error jsonp_init();
Error jsonp_init_1(uint64_t config_flags);
Error jsonp_init_2(uint64_t config_flags, uint32_t max_depth);
Error jsonp_init_3(uint64_t config_flags, uint32_t max_depth, char const * whitespace_chars);



// call when done with parsing module, frees up resources acquired in init().
void jsonp_destroy(void);



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
 *    form feed (’\f’),
 *    vertical tab (’\v’)
 *  These are not included by default as white space characters in this parser.
 *
 *  todo (rob) this currently is limited to ASCII characters. Should we support any Unicode codepoint?
 *
 * @param whitespace_chars the characters that should be treated as white space characters.
 *
 */
void jsonp_define_whitespace_chars( const char  *whitespace_chars );

/* Main parsing functions */
JsonValue *jsonp_parse(const char *json_text, JsonError *error, Arena *arena);

// version that takes an argument, buffer_size, which is the actual size of the JSON text buffer in bytes.
// this method can report errors where it parsed successfully but did not use up the entire buffer
JsonValue *jsonp_parse_ex(const char *json, JsonError *error, Arena *arena, uint32_t buffer_size);

// Searches the entries in the JSON object `json_obj` and returns the entry whose key matches the argument `key`.
// Returns nullptr if there is no entry with this key.
JsonObjectEntry * jsonp_entry_for_key(const JsonValue *json_obj, char const * key) ;

// print a string representation of the JSON graph to the console
void jsonp_print_json_value(JsonValue *value);

/**
 *  Sets the JsonConfigFlag flags in a uint6t_t and returns it them to the caller.
 *  The value returned from this function can be used as
 *  the json_init() method's `config_flags` argument.
 *  Example:
 *     uint64_t my_custom_flags =
 *      jsonp_set_config_flags( 3, (JsonConfigFlag[3]) {
 *          JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_ARRAYS,
 *          JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_OBJECTS,
 *          JSON_CONFIG_ALLOW_UNICODE_U_ESCAPE
 *     });
 *     Error err = jsonp_init_1(my_custom_flags);
 *     ...
 *
 */
uint64_t jsonp_set_config_flags( uint32_t flag_count, JsonConfigFlag const *flag);

bool jsonp_is_flag_set(JsonConfigFlag flag);

void jsonp_set_config_flag(JsonConfigFlag flag);


void jsonp_clear_config_flag(JsonConfigFlag flag);

/**
 *  Sets the maximum nesting depth allowed in the JSON text. If depth is exceeded, the JSON text is
 *  rejected as invalid.
 *  The default is specified in DEPTH_MAX_DEFAULT
 *  @param depth the maximum allowed nesting depth of the JSON text structure.
 */
void jsonp_set_max_nesting_depth( uint32_t depth );

void json_value_repr(JsonValue *value);

#ifdef __cplusplus
}
#endif

#endif // JSON_PARSER_H
