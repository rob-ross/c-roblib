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

// -----------------------------------------------------------------
//      REGULAR EXPRESSION STRINGS
// -----------------------------------------------------------------

// RSL: "Regex Start of Line"
#define RSL      "^"
#define WB_START "[[:<:]]"
#define WB_END   "[[:>:]]"
#define SP       "[[:space:]]*"
#define WS       "[\x20\x09\x0A\x0D]"
#define WS_star  "[\x20\x09\x0A\x0D]*"

static const char * const REGEX_TRUE   = RSL WS_star WB_START "true"  WB_END WS_star;
static const char * const REGEX_FALSE  = RSL WS_star WB_START "false" WB_END WS_star;
static const char * const REGEX_NULL   = RSL WS_star WB_START "null"  WB_END WS_star;
static const char * const REGEX_STRING = RSL WS_star "\x22.*\x22" WS_star;
static const char * const REGEX_NUMBER = RSL WS_star "(-?(0|[1-9])[0-9]*(\\.[0-9]+)?([eE][-+]?[0-9]+)?)" WS_star;

static const char * const REGEX_LEFT_BRACKET   = WS_star "["  WS_star;
static const char * const REGEX_RIGHT_BRACKET  = WS_star "]"  WS_star;

constexpr int NUM_REGEX_PATTERNS = 5;
RegexPattern REGEX_TRUE_PATTERN    = { .pattern_string  =  REGEX_TRUE,   .name="true",   .token = TOK_TRUE  };
RegexPattern REGEX_FALSE_PATTERN   = { .pattern_string  =  REGEX_FALSE,  .name="false",  .token = TOK_FALSE };
RegexPattern REGEX_NULL_PATTERN    = { .pattern_string  =  REGEX_NULL,   .name="null",   .token = TOK_NULL  };
RegexPattern REGEX_STRING_PATTERN  = { .pattern_string  =  REGEX_STRING, .name="string", .token = TOK_STRING  };
RegexPattern REGEX_NUMBER_PATTERN  = { .pattern_string  =  REGEX_NUMBER, .name="number", .token = TOK_NUMBER  };


// can't do this at compile time as we can in Python and Java, must do it in the init method.
// patterns = {REGEX_TRUE_PATTERN, REGEX_FALSE_PATTERN, REGEX_NULL_PATTERN};
RegexPattern *patterns[NUM_REGEX_PATTERNS];

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
typedef struct json_object_entry json_object_entry;

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
            json_object_entry *entries;
            size_t count;
        } object;
    } u;
};

struct json_object_entry {
    char *key;
    JsonValue *value;
};

typedef struct json_error_t {
    const char *message;
    int line;
    int column;
    int parse_start;
    int parse_end;
} JsonError;


constexpr char QUOTATION_MARK = '"';

Error jsonp_init();
/* Main parsing function */
JsonValue *json_parse(const char *json, JsonError *error);

/* Recursive cleanup */
void json_value_free(JsonValue *value);


void json_repr(JsonValue *value);

#endif // JSON_PARSER_H


