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

#include <regex.h>
// #include "base_types.h"

#include "arena.h"
#include "error_result.h"

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

typedef struct json_error_s {
    const char *message;
    const char *json;
    int first_bad_char; // position where parsing failed
    int line;
    int column;
    int parse_start;
    int parse_end;
} JsonError;



// Must call at application startup to initialize the parser before first use.
Error jsonp_init();
// call when done with parsing module, frees up resources acquired in init().
void jsonp_destroy(void);

/* Main parsing function */
JsonValue *json_parse(const char *json, JsonError *error, Arena *arena);

// Searches the entries in the JSON object `json_obj` and returns the entry whose key matches the argument `key`.
// Returns nullptr if there is no entry with this key.
JsonObjectEntry * jsonp_entry_for_key(const JsonValue *json_obj, char const * key) ;

// print a string representation of the JSON graph to the console
void json_value_str(JsonValue *value);


void json_value_repr(JsonValue *value);

#ifdef __cplusplus
}
#endif

#endif // JSON_PARSER_H



/*
 *
Why validation is important (what you might be missing):
    ◦ Invalid Byte Sequences: UTF-8 has specific rules for how bytes combine. For example, a "continuation byte" (0x80-0xBF)
must always follow a "start byte" (0xC0-0xF4, excluding 0xC0, 0xC1, 0xF5-0xFF).
If you encounter a continuation byte without a preceding start byte, or an invalid sequence like 0xC0 0x80,
it's malformed UTF-8. A parser that doesn't validate would just pass these bytes through, potentially leading
to garbage output, security issues, or crashes in downstream applications.
    ◦ Overlong Encodings: UTF-8 has a canonical form. For example, the ASCII character A (U+0041) should always
be encoded as 0x41. It could theoretically be encoded as a two-byte sequence (0xC1 0x81), but this is
an "overlong encoding" and is forbidden in UTF-8. Overlong encodings can be used in security exploits
to bypass filters that only check for the single-byte ASCII representation of a character.
A validating parser would reject this.
    ◦ Surrogate Pairs (D800-DFFF): Unicode reserves the range U+D800 to U+DFFF for "surrogate pairs,"
which are used in UTF-16 to represent characters outside the Basic Multilingual Plane (BMP).
These code points should never appear directly in valid UTF-8. If they do, it's malformed.
    ◦ Non-characters: Certain Unicode code points are designated as "non-characters" (e.g., U+FFFE, U+FFFF).
While not strictly forbidden by UTF-8 encoding rules, a robust parser might choose to flag or reject them,
as they often indicate errors.
    ◦ Security and Interoperability: Passing through invalid UTF-8 can lead to:
▪ Denial of Service: Malformed input could crash applications that later try to process the "string."
▪ Data Corruption: Downstream systems might misinterpret the bytes.
▪ Security Vulnerabilities: As mentioned with overlong encodings, it can bypass security checks.
4.
What "handling Unicode characters" truly means for a parser:
For a JSON parser, "handling Unicode characters" means:
    ◦ Reading raw bytes: As you're doing with fopen("rb").
    ◦ Identifying string boundaries: Finding the opening and closing quotes.
    ◦ Processing \uXXXX escapes: Converting these 6-character sequences into their corresponding Unicode code points.
    ◦ Validating UTF-8 sequences: For any bytes within a string that are not part of a \uXXXX escape, ensuring they form
valid UTF-8 sequences. If they don't, the parser should report an error.
    ◦ Providing a consistent internal representation: Once validated, your parser might convert these UTF-8 sequences
into a different internal representation (e.g., UTF-32 code points, or a custom wchar_t based string if your system
supports it) for easier manipulation, or it might keep them as UTF-8 bytes but ensure their validity.



Encoding Surrogate Pairs:
Example: U+1F600 (Grinning Face Emoji 😀)
Let's take U+1F600.
• Subtract 0x10000 from the code point: 0x1F600 - 0x10000 = 0xF600.
• Split the 20-bit result into two 10-bit halves:
    ◦ High 10 bits: 0x0000 to 0x03FF (0xF600 >> 10 = 0x003D)
    ◦ Low 10 bits: 0x0000 to 0x03FF (0xF600 & 0x3FF = 0x0200)
• Add 0xD800 to the high 10 bits to get the high surrogate: 0xD800 + 0x003D = 0xD83D.
• Add 0xDC00 to the low 10 bits to get the low surrogate: 0xDC00 + 0x0200 = 0xDE00.

So, the Unicode character U+1F600 is represented in JSON as the sequence: \uD83D\uDE00
What your parser needs to do:
When your JSON parser encounters a \uXXXX escape sequence:
1. It should parse the four hex digits and get the 16-bit value.
2. If this 16-bit value is in the high surrogate range (U+D800 to U+DBFF), your parser must then expect the next character sequence to be another \uXXXX escape.
3. It should parse the second \uXXXX and check if it's in the low surrogate range (U+DC00 to U+DFFF).
4. If both conditions are met, it combines these two 16-bit surrogates back into a single 20-bit (or 21-bit, up to U+10FFFF) Unicode code point.
    ◦ The formula to combine them is: ((high_surrogate - 0xD800) << 10) + (low_surrogate - 0xDC00) + 0x10000.
5. If a high surrogate is not followed by a low surrogate, or vice-versa, that's an error in the JSON string according to the spec.
 */


/**

My notes.

1. Unicode code points 0x00     - 0x7F     are the same as an ASCII byte.
2. Unicode code points 0x80     - 0x07FF   are two-byte sequences
3. Unicode code points 0x0800   - 0xFFFF   are three-byte sequences
4  Unicode code points 0x010000 - 0x10FFFF are four-byte sequences

1. If the first byte in a sequence starts with 0, it's a 1-byte encoded codepoint
2. If the first byte in a sequence starts with 110xxxxx (binary), it's a two-byte sequence
3. If the first byte in a sequence starts with 1110xxxx (binary), it's a three-byte sequence
4. If the first byte in a sequence starts with 1111xxxx (binary), it's a four-byte sequence

In any multibyte sequence, all bytes after the first must start with 10xxxxxx (binary)

U+D800 - U+DFFF: this range does not encode any Unicode codepoints. This range is used for surrogate pairs when
                  encoding in UTF-16. If the JSON string contains Unicode escapes \uD800-\uDFFF, there must be
                two such escape sequences and must be decoded as UTF-16 surrogate pairs.

*/
