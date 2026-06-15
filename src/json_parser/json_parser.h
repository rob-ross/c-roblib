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
begin-array     = ws %x5B ws  ; [ left square bracket
begin-object    = ws %x7B ws  ; { left curly bracket
end-array       = ws %x5D ws  ; ] right square bracket
end-object      = ws %x7D ws  ; } right curly bracket
name-separator  = ws %x3A ws  ; : colon
value-separator = ws %x2C ws  ; , comma

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
#include <_regex.h>
#include <stdbool.h>

#include "error_result.h"

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
    TOK_STRING, // this is actually a value not a token.... but so are true, false, and null. hmmm....???
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

typedef struct json_error_s {
    const char *message;
    const char *json;
    int first_bad_char; // position where parsing failed
    int line;
    int column;
    int parse_start;
    int parse_end;
} JsonError;




Error jsonp_init();
void jsonp_destroy(void);

/* Main parsing function */
JsonValue *json_parse(const char *json, JsonError *error);

// Searches the entries in the JSON object `json_obj` and returns the entry whose key matches the argument `key`.
// Returns nullptr if there is no entry with this key.
JsonObjectEntry * jsonp_entry_for_key(const JsonValue *json_obj, char const * key) ;

// print a string representation of the JSON graph to the console
void json_value_str(JsonValue *value);


void json_value_repr(JsonValue *value);

#endif // JSON_PARSER_H


