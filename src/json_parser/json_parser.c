// json_parser.c
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/06/02 01:37:49 PDT

/*
 *
*The "Master Prompt" for Generation
To get a high-quality, compliant parser from an AI, you need to be specific about the architecture and the edge cases. Here is the prompt I recommend:
"Act as a world-class systems engineer. Write a thread-safe, RFC 8259 compliant JSON parser in C.
Requirements:
1.
Architecture: Use a recursive descent parsing strategy.
2.
Data Structures: Define a json_value struct using a tagged union to represent Objects, Arrays, Strings, Numbers, Booleans, and Null.
3.
Memory: Provide a json_value_free function to recursively clean up the AST. Do not use global state.
4.
Strings: Correctly handle all escape sequences, including Unicode \uXXXX and UTF-8 encoding.
5.
Numbers: Support integers, fractions, and exponents as defined by the RFC.
6.
Error Handling: Return detailed error information, including the line number, column number, and a descriptive message.
7.
Constraints: Use only the C standard library (C99 or later).
8.
Interface: Provide a json_parse(const char *input) function that returns a pointer to the root json_value."
 **/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <stdio.h>

#include "json_parser.h"
#include "arena.h"
#include "error_result.h"
#include "../string_utils.h"

typedef struct {
    const char *json;
    int line;
    int column;
} json_context;


Error jsonp_init();



static void skip_whitespace(json_context *ctx) {
    while (*ctx->json && isspace(*ctx->json)) {
        if (*ctx->json == '\n') {
            ctx->line++;
            ctx->column = 1;
        } else {
            ctx->column++;
        }
        ctx->json++;
    }
}

static Arena arena = {};

constexpr int MATCH_FOUND = 0;


/* Implementation of specific parsers would go here (parse_string, parse_number, etc) */

static int match_one_pattern( const RegexPattern *rp, char const * str) {
    constexpr size_t max_groups = 4;
    // The first element (0) is the entire match, subsequent elements are capture groups
    regmatch_t pmatch[max_groups] = {}; // Assuming max_groups - 1 capture groups + full match
    const int result = regexec( &rp->compiled_regex, str, max_groups, pmatch, 0);
    if ( result == MATCH_FOUND) {
        // debug
        // printf("match found for %s\n",rp->name);
        return (int)pmatch[0].rm_eo;
    } else {
        printf("   no match found for '%s' with pattern %s\n", str, rp->pattern_string);
        return -1;
    }
}

static bool parse_null(json_context *ctx, json_error *error) {
    int match_len = match_one_pattern(&REGEX_NULL_PATTERN, ctx->json);
    if (match_len < 0 ) {
        error->column = ctx->column;
        error->line   = ctx->line;
        // we need a substring command so we can limit the message string to the relevant characters and not the whole string
        error->message = sutil_concat_strings("expected 'null', got: ", ctx->json, nullptr);
        return false;
    }
    ctx->json   += match_len;
    ctx->column += match_len;
    return true;
}



static bool parse_true(json_context *ctx, json_error *error) {
    int match_len = match_one_pattern(&REGEX_TRUE_PATTERN, ctx->json);
    if (match_len < 0 ) {
        error->column = ctx->column;
        error->line   = ctx->line;
        // we need a substring command so we can limit the message string to the relevant characters and not the whole string
        error->message = sutil_concat_strings("expected 'true', got: ", ctx->json, nullptr);
        return false;
    }
    ctx->json += match_len;
    ctx->column += match_len;
    return true;

}

static bool parse_false(json_context *ctx, json_error *error) {
    int match_len = match_one_pattern(&REGEX_FALSE_PATTERN, ctx->json);
    if (match_len < 0 ) {
        error->column = ctx->column;
        error->line   = ctx->line;
        // we need a substring command so we can limit the message string to the relevant characters and not the whole string
        error->message = sutil_concat_strings("expected 'false', got: ", ctx->json, nullptr);
        return false;
    }
    printf("match found for %s\n",ctx->json);
    ctx->json += match_len;
    ctx->column += match_len;
    return true;

}

constexpr char QUOTE           = 0x22;  // "
constexpr char REVERSE_SOLIDUS = 0x5c;  // \  backslash
constexpr char LEFT_BRACKET    = '[';
constexpr char RIGHT_BRACKET   = ']';
constexpr char LEFT_BRACE      = '{';
constexpr char RIGHT_BRACE     = '}';
constexpr char COLON           = ':';
constexpr char COMMA           = ',';

static bool parse_string(json_context *ctx, json_error *error) {
    // the standard regex C library doesn't provide look-arounds. so simple regex matching with escaped quotes
    // causes the string to terminate early when there's not a closing, non-escaped quote.
    // E.g., ' "This  \"json\" string has embedded quotes but no matching closing quote ':
    // this causes the matched text to be "This  \"json\", which is an unterminated string, but leaves the rest
    // of the unterminated string in the buffer.

    // we examine chars in the stream until we find:
    // 1. a closing quote, which is a quote not preceeded by the backlash (reverse solidus)
    // 2. EOF, AKA null terminator, \x00
    // 3. any of the delimiter chars bracket, brace, colon, comma.

    const char *json_ptr = ctx->json + 1; // Skip the opening quote
    while (*json_ptr) {
        if (*json_ptr == QUOTE) {
            // happy case. We found the terminating quote
            int match_len = (int)((json_ptr + 1) - ctx->json);
            ctx->json   += match_len;
            ctx->column += match_len;
            return true;
        }

        if (*json_ptr == REVERSE_SOLIDUS) {
            json_ptr++; // Move to the escaped character
            if (*json_ptr == '\0') {
                // printf(" Unexpected EOF after backslash\n");
                error->column = ctx->column;
                error->line   = ctx->line;
                error->message = "Unexpected EOF after backslash";
                return false; // Unexpected EOF
            }
            
            // Validate escape sequence
            switch (*json_ptr) {
                case '"': case '\\': case '/': case 'b':
                case 'f': case 'n': case 'r': case 't':
                    break;
                case 'u':
                    // RFC 8259: \u followed by 4 hex digits
                    for (int i = 0; i < 4; i++) {
                        json_ptr++;
                        if (*json_ptr == '\0') {
                            // printf("Unexpected EOF in Unicode escape\n");
                            error->column = ctx->column;
                            error->line   = ctx->line;
                            error->message = "Unexpected EOF in Unicode escape";
                            return false;
                        }
                        if (!isxdigit((unsigned char)*json_ptr)) {
                            // printf("Invalid hex digit in Unicode escape: %c\n", *json_ptr);
                            error->column = ctx->column;
                            error->line   = ctx->line;
                            error->message =  sutil_concat_strings("Invalid hex digit in Unicode escape: ", json_ptr, nullptr);
                            return false;
                        }
                    }
                    break;
                default:
                    error->column = ctx->column;
                    error->line   = ctx->line;
                    error->message =  sutil_concat_strings("Invalid escape sequence: \\", json_ptr, nullptr);
                    // printf(" Invalid escape sequence: \\%c\n", *json_ptr);
                    return false;
            }
        } else if ((unsigned char)*json_ptr <= 0x1F) {
            // RFC 8259: Control characters U+0000 through U+001F MUST be escaped.
            // This means the literal bytes cannot appear here.
            error->column = ctx->column;
            error->line   = ctx->line;
            printf(" Unexpected unescaped control character: 0x%.2X\n", (unsigned char)*json_ptr);
            return false;
        }

        // switch (*json_ptr) {
        //     case LEFT_BRACKET:
        //     case RIGHT_BRACKET:
        //     case LEFT_BRACE:
        //     case RIGHT_BRACE:
        //     case COLON:
        //     case COMMA: {
        //         // These are actually valid characters inside a JSON string
        //         // unless they are outside the quotes.
        //         break;
        //     }
        // }

        json_ptr++;
    }

    error->column = ctx->column;
    error->line   = ctx->line;
    error->message = "No closing quote found.";
    return false;  // no closing quote found

}




static json_value *parse_value(json_context *ctx, json_error *error) {
    skip_whitespace(ctx);
    json_value *value = nullptr;
    switch (*ctx->json) {
        case 'n': /* Handle null */ {
            if (parse_null(ctx, error)) {
                // do something here.
                value = arena_alloc(&arena, sizeof(json_value) );
                value->type = JSON_NULL;
                printf("We matched a 'null' in the input string.\n");
            }
            break;
        }
        case 't': /* Handle true */ {
            if (parse_true(ctx, error)) {
                // do something here.
                value = arena_alloc(&arena, sizeof(json_value) );
                value->type = JSON_BOOLEAN;
                value->u.boolean = true;
                printf("We matched a 'true' in the input string.\n");

            }
            break;
        }
        case 'f': /* Handle false */ {
            if (parse_false(ctx, error)) {
                // do something here.
                value = arena_alloc(&arena, sizeof(json_value) );
                value->type = JSON_BOOLEAN;
                value->u.boolean = false;
                printf("We matched a 'false' in the input string.\n");
            }
            break;
        }
        case '"': /* Handle string */
            if ( parse_string(ctx, error) ) {
                value = arena_alloc(&arena, sizeof(json_value) );
                value->type = JSON_STRING;
                value->u.string = ctx->json; // we need to know how big the str is so we can copy it here.
                printf("We matched a json string in the input string.\n");
            }
            break;
        case '[': /* Handle array */
        case '{': /* Handle object */
        case '-': case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7': case '8': case '9':
            /* Handle number */
        default:
            if (error) {
                error->message = "Unexpected character";
                error->line = ctx->line;
                error->column = ctx->column;
            }
            return nullptr;
    }


    return value;

}

json_value *json_parse(const char *json, json_error *error) {
    if (!json) return nullptr;
    json_context ctx = {json, 1, 1};
    json_value *value =  parse_value(&ctx, error);

    return value;
}

void json_value_free(json_value *value) {
    if (!value) return;

    switch (value->type) {
        case JSON_STRING:
            free((void*)value->u.string);
            break;
        case JSON_ARRAY:
            for (size_t i = 0; i < value->u.array.count; i++) {
                json_value_free(value->u.array.elements[i]);
            }
            free(value->u.array.elements);
            break;
        case JSON_OBJECT:
            for (size_t i = 0; i < value->u.object.count; i++) {
                free(value->u.object.entries[i].key);
                json_value_free(value->u.object.entries[i].value);
            }
            free(value->u.object.entries);
            break;
        default:
            break;
    }
    free(value);
}

constexpr int COMPILE_SUCCESS = 0;


static bool is_initialized = false;

Error init_regex_patterns(void) {
    RegexPattern *p_list[] = { &REGEX_TRUE_PATTERN, &REGEX_FALSE_PATTERN, &REGEX_NULL_PATTERN, &REGEX_STRING_PATTERN };

    for (int i = 0; i < NUM_REGEX_PATTERNS; i++) {
        int reti = regcomp(&p_list[i]->compiled_regex, p_list[i]->pattern_string, REG_EXTENDED);
        if ( reti != COMPILE_SUCCESS) {
            char msgbuf[100];
            regerror(reti, &p_list[i]->compiled_regex, msgbuf, sizeof(msgbuf));
            fprintf(stderr, "Regex compilation failed for '%s': %s\n", p_list[i]->pattern_string, msgbuf);
            Error result =  (Error){ .err = true, .reported_err = reti, .err_obj = p_list[i] };
            strncpy(result.msg, msgbuf, sizeof msgbuf);
            result.msg[ sizeof msgbuf - 1 ] = '\0';
            return result; // Exit early on compilation error
        }
        patterns[i] = p_list[i]; // keep a copy in the array
    }

    return (Error){};
}

Error jsonp_init() {

    // compile regex
    Error e = init_regex_patterns();
    if (e.err) {
        return e;
    }

    // allocate arena
    ArenaErrResult aer = arena_create_arena( &arena, ONE_MIBIBYTE * 100);
    if ( aer.err ) {
        printf("arena_create_arena failed with %d, %s\n", aer.reported_err, aer.msg);
    }
    is_initialized = true;
    return (Error){ .error = aer.error };
}

void jsonp_destroy(void) {
    regfree(&REGEX_TRUE_PATTERN.compiled_regex);
    regfree(&REGEX_FALSE_PATTERN.compiled_regex);
    regfree(&REGEX_NULL_PATTERN.compiled_regex);
    arena_destroy_arena(&arena);
    arena = (Arena){};
    is_initialized = false;
}

void test_parse_str(char const * str) {
    json_error err_obj = {};
    // printf("\nParsing json string '%s': \n", str);
    json_value *jval = json_parse(str, &err_obj);
    if (!jval) {
        // printf("  json_parse returns nullptr\n" );
        printf("line:%d col:%d  %s\n", err_obj.line, err_obj.column, err_obj.message);
    }
    else {
        // printf("  json_type=%d\n", jval->type);
    }
}

int main( ) {
    Error err = jsonp_init();
    if (err.err) {
        err_print(err);
        return err.reported_err;
    }

    // test_parse_str(" null ");
    // test_parse_str("true");
    // test_parse_str("false");
    // test_parse_str("");
    test_parse_str("^%$the heck is this mess?");
    //
    // test_parse_str(nullptr);

    test_parse_str("nullington");
    //
    // test_parse_str("             true");
    // test_parse_str("             true           ");

    // test string parsing

    // test_parse_str(" \"This is a json string\" ");
    // test_parse_str(" \"This is a 'json' string too!\" ");
    // test_parse_str(" \"This  \\\"json\\\" string has embedded quotes\" ");

    // test_parse_str(" \"This  \\\"json\\\" string has embedded quotes but no matching closing quote ");
    //
    // test_parse_str(" \"This  \\\"json\\\" string has embedded quotes but no matching closing quote [ but it has a delimeter");
    // test_parse_str(" \"This  \\\"json\\\" string has embedded quotes but no matching closing quote ] but it has a delimeter");
    // test_parse_str(" \"This  \\\"json\\\" string has embedded quotes but no matching closing quote { but it has a delimeter");
    // test_parse_str(" \"This  \\\"json\\\" string has embedded quotes but no matching closing quote } but it has a delimeter");
    // test_parse_str(" \"This  \\\"json\\\" string has embedded quotes but no matching closing quote : but it has a delimeter");
    // test_parse_str(" \"This  \\\"json\\\" string has embedded quotes but no matching closing quote , but it has a delimeter");
    //
    // test_parse_str(" \"This  \\\"json\\\" string has no\nclosing quote , but it has a delimeter and newline");

    test_parse_str(" \"This string has a newline here->\n that is not escaped.  \"");

    test_parse_str(" \"This string has a newline 0x13 here->\\\n that IS escaped.  \"");

    test_parse_str(" \"This string has an escape-n newline here->\\n that IS escaped.  \"");

    jsonp_destroy();

    return 0;
}
