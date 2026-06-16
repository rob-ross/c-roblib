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

#include <assert.h>

#include "arena.h"
#include "error_result.h"
#include "../string/string_utils.h"
#include "../string/string_builder.h"

typedef struct json_context_t {
    const char *current_ptr;  // The current text being parsed, advances through the JSON text in the json member
    const char *json; // full original JSON text string
    int line;
    int column;
    int parse_start;
    int parse_end;
} JsonContext;



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

static Arena arena = {};
constexpr uint32_t ERROR_MSG_BUFFER_SIZE = 1024;
static char error_msg_buffer[ERROR_MSG_BUFFER_SIZE] = {};

constexpr int MATCH_FOUND = 0;
constexpr char QUOTATION_MARK = '"';

// -----------------------------------------------------------------
//      Forward References
// -----------------------------------------------------------------
static JsonValue *parse_value(JsonContext *context, JsonError *error) ;



// Returns the current position of the parser as an index into context->json
static int parse_position( const JsonContext *context) {
    return (int)(context->current_ptr - context->json);
}

static void skip_whitespace(JsonContext *context) {
    while (*context->current_ptr && isspace(*context->current_ptr)) {
        if (*context->current_ptr == '\n') {
            context->line++;
            context->column = 1;
        } else {
            context->column++;
        }
        context->current_ptr++;
    }
}


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
        // printf("   no match found for '%s' with pattern %s\n", str, rp->pattern_string);
        return -1;
    }
}

static inline int max_int(const int a, const int b) {
    return a > b ? a : b;
}

static inline int min_int(const int a, const int b) {
    return a < b ? a : b;
}

// advance the parser state based on the current parse window
static void advance(JsonContext *context, const int char_count) {
    context->current_ptr += char_count;
    context->column       += char_count;
    context->parse_end    = context->parse_start + char_count;
}

static void record_error(const JsonContext *context, JsonError *error, const int char_count, const char *msg) {
    error->column = context->column;
    error->line   = context->line;
    error->parse_start = context->parse_start;
    error->parse_end = context->parse_end;
    error->first_bad_char = parse_position(context);

    // error->parse_end = error->parse_start + min_int(char_count, (int)strlen(context->current_ptr));
    // if (context->current_ptr[error->parse_end - 1] != '\0') {
    //     error->parse_end++; // so we can display the character after the parse failure
    // }
    error->message = msg;
}


static JsonValue * parse_null(JsonContext *context, JsonError *error) {
    JsonValue *value = nullptr;

    int match_len = match_one_pattern(&REGEX_NULL_PATTERN, context->current_ptr);
    if (match_len < 0 ) {
        // we need a substring command so we can limit the message string to the relevant characters and not the whole string
        record_error(context, error,  4, "");
        const int substr_len = error->parse_end - error->parse_start;
        snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE, "expected 'null', got: %.*s",
            substr_len, context->current_ptr);
        error->message = error_msg_buffer;
        return value;
    }

    value = arena_alloc(&arena, sizeof(JsonValue) );
    value->type = JSON_NULL;
    advance(context, match_len);
    return value;
}



static JsonValue *  parse_true(JsonContext *context, JsonError *error) {
    JsonValue *value = nullptr;

    int match_len = match_one_pattern(&REGEX_TRUE_PATTERN, context->current_ptr);
    if (match_len < 0 ) {
        record_error(context, error, 4, "");
        const int substr_len = error->parse_end - error->parse_start;
        snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE, "expected 'true', got: %.*s",
            substr_len, context->current_ptr);
        error->message = error_msg_buffer;
        return value;
    }

    value = arena_alloc(&arena, sizeof(JsonValue) );
    value->type = JSON_BOOLEAN;
    value->u.boolean = true;
    advance(context, match_len);
    return value;

}

static JsonValue *  parse_false(JsonContext *context, JsonError *error) {
    JsonValue *value = nullptr;

    int match_len = match_one_pattern(&REGEX_FALSE_PATTERN, context->current_ptr);
    if (match_len < 0 ) {
        record_error(context, error, 5, "");
        const int substr_len = error->parse_end - error->parse_start;
        snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE, "expected 'false', got: %.*s",
            substr_len, context->current_ptr);
        error->message = error_msg_buffer;
        return value;
    }

    value = arena_alloc(&arena, sizeof(JsonValue) );
    value->type = JSON_BOOLEAN;
    value->u.boolean = false;
    advance(context, match_len);
    return value;

}

constexpr char QUOTE           = 0x22;  // "
constexpr char REVERSE_SOLIDUS = 0x5c;  // \  backslash
constexpr char LEFT_BRACKET    = '[';
constexpr char RIGHT_BRACKET   = ']';
constexpr char LEFT_BRACE      = '{';
constexpr char RIGHT_BRACE     = '}';
constexpr char COLON           = ':';
constexpr char COMMA           = ',';

static JsonValue * parse_string(JsonContext *context, JsonError *error) {
    // the standard regex C library doesn't provide look-arounds. so simple regex matching with escaped quotes
    // causes the string to terminate early when there's not a closing, non-escaped quote.
    // E.g., ' "This  \"json\" string has embedded quotes but no matching closing quote ':
    // this causes the matched text to be "This  \"json\", which is an unterminated string, but leaves the rest
    // of the unterminated string in the buffer.

    StringBuilder sb;
    sb_init(&sb, 16, "");

    // we examine chars in the stream until we find:
    // 1. a closing quote, which is a quote not preceded by the backlash (reverse solidus)
    // 2. EOF, AKA null terminator, \x00
    // 3. any of the delimiter chars bracket, brace, colon, comma.
    JsonValue *value = nullptr;
    advance(context, 1); // Skip the opening quote
    const char *json_ptr = context->current_ptr;
    while (*json_ptr) {
        if (*json_ptr == QUOTE) {
            // happy case. We found the terminating quote
            int match_len = (int)((json_ptr) - context->current_ptr);


            value = arena_alloc(&arena, sizeof(JsonValue) );
            value->type = JSON_STRING;

            // previous method, extracting from the json_str
            // size_t n = snprintf(nullptr, 0, "%.*s", match_len, context->current_ptr);
            // value->u.string = arena_alloc(&arena, n + 1);
            // // todo error checking
            // snprintf((char *)value->u.string, n + 1, "%.*s", match_len, context->current_ptr);

            void * str_value = arena_alloc(&arena, sb.length + 1);
            memcpy(str_value, sb.buffer, sb.length + 1);
            value->u.string = str_value;


            advance(context, match_len + 1); // we add 1 to consume the terminating quote

            // printf("  returning from parse_str, StringBuilder buffer is : '%s'\n", sb.buffer);

            sb_destroy(&sb);
            return value;
        }

        if (*json_ptr == REVERSE_SOLIDUS) {
            json_ptr++; // Move to the escaped character
            if (*json_ptr == '\0') {
                // printf(" Unexpected EOF after backslash\n");
                record_error(context, error, 2, "Unexpected EOF after backslash");

                sb_destroy(&sb);
                return nullptr; // Unexpected EOF
            }
            
            // Validate escape sequence
            switch (*json_ptr) {
                case '"':
                case '\\':
                case '/':
                    sb_append_char(&sb, *json_ptr);
                    break;
                case 'b':
                    sb_append_char(&sb, '\b');
                    break;
                case 'f':
                    sb_append_char(&sb, '\f');
                    break;
                case 'n':
                    sb_append_char(&sb, '\n');
                    break;
                case 'r':
                    sb_append_char(&sb, '\r');
                    break;
                case 't':
                    sb_append_char(&sb, '\t');
                    break;
                case 'u':
                    // RFC 8259: \u followed by 4 hex digits
                    for (int i = 0; i < 4; i++) {
                        json_ptr++;
                        if (*json_ptr == '\0') {
                            // printf("Unexpected EOF in Unicode escape\n");
                            record_error(context, error, 6, "Unexpected EOF after Unicode escape");
                            sb_destroy(&sb);
                            return nullptr;
                        }
                        if (!isxdigit((unsigned char)*json_ptr)) {
                            // printf("Invalid hex digit in Unicode escape: %c\n", *json_ptr);
                            snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE, "Invalid hex digit in Unicode escape: %c", *json_ptr);
                            // error->message =  sutil_concat_strings("Invalid hex digit in Unicode escape: ", json_ptr, nullptr);
                            record_error(context, error, 2, error_msg_buffer);
                            sb_destroy(&sb);
                            return nullptr;
                        }
                    }
                    break;
                default:

                    snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE, "Invalid escape sequence: \\%c", *json_ptr);
                    record_error(context, error, 2, error_msg_buffer);
                    sb_destroy(&sb);
                    return nullptr;
            }
        } else if ((unsigned char)*json_ptr <= 0x1F) {
            // RFC 8259: Control characters U+0000 through U+001F MUST be escaped.
            // This means the literal bytes cannot appear here.
            snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE, "Unexpected unescaped control character: 0x%.2X\n", (unsigned char)*json_ptr);
            record_error(context, error, 2, error_msg_buffer);
            sb_destroy(&sb);
            return nullptr;
        } else {
            sb_append_char(&sb, *json_ptr);  // capture current char into the StringBuilder
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

    record_error(context, error, ERROR_MSG_BUFFER_SIZE, "No closing quote found.");
    sb_destroy(&sb);
    return nullptr;  // no closing quote found

}

static JsonValue * parse_number(JsonContext *context, JsonError *error) {
    JsonValue *value = nullptr;

    constexpr size_t max_groups = 4;
    // The first element (0) is the entire match, subsequent elements are capture groups
    regmatch_t pmatch[max_groups] = {}; // Assuming max_groups - 1 capture groups + full match
    const int result = regexec( &REGEX_NUMBER_PATTERN.compiled_regex, context->current_ptr, max_groups, pmatch, 0);
    int match_len =  (int)pmatch[0].rm_eo;
    if (result != MATCH_FOUND || match_len < 0) {
        snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE, "expected number, got: %.*s", 100 ,context->current_ptr);
        record_error(context, error, ERROR_MSG_BUFFER_SIZE, error_msg_buffer);
        return nullptr;
    }
    auto start = pmatch[1].rm_so;
    auto end = pmatch[1].rm_eo;
    bool is_double = false;
    for (regoff_t i = start; i < end; ++i) {
        // printf(" , %lld:%c", i, context->json[i]);
        if (context->current_ptr[i]=='.') {
            is_double = true;
            break;
        }
    }
    // if (is_double) {
    //     printf("the number is a double.\n");
    // } else {
    //     printf("the number is an integer.\n");
    // }


    // capture group 1 should have our number string

    // printf("pmatch[1].rm_so=%lld, pmatch[1].rm_eo=%lld\n", pmatch[1].rm_so ,pmatch[1].rm_eo);

    // printf("number match found for %s\n",context->current_ptr);

    value = arena_alloc(&arena, sizeof(JsonValue) );
    value->type = JSON_NUMBER;
    if (is_double) {
        value->type = JSON_FLOAT;
        value->u.n_double = strtod(context->current_ptr, nullptr);
    } else {
        value->type = JSON_INT;
        value->u.n_long = strtol(context->current_ptr, nullptr, 10);

    }
    advance(context, match_len);
    return value;


}

typedef struct json_value_node_s {
    JsonValue *value;
    struct json_value_node_s *next;
} JsonValueNode;

// add a node to the linked list headed by first_node.
// adds nodes to the front of the linked list. If first_node is null, will be the head node of a new linked list.
// Returns the new first_node.
static JsonValueNode * add_json_value_node(JsonValueNode * first_node, JsonValue *value) {
    if (!value) return nullptr;
    JsonValueNode * new_node = (JsonValueNode*) arena_alloc(&arena, sizeof(JsonValueNode) );
    new_node->value = value;
    new_node->next = first_node;
    return new_node;
}


static JsonValue * parse_array(JsonContext *context, JsonError *error) {
    // we recursively parse elements of this array until we see end-of-array ']' char
    JsonValue *array =  arena_alloc(&arena, sizeof(JsonValue) );

    size_t num_elements = 0;
    array->type = JSON_ARRAY;
    array->u.array.count = num_elements;
    array->u.array.elements = nullptr;

    JsonValueNode *elements = nullptr;
    advance(context, 1);  // consume '['
    skip_whitespace(context);

    while ( *context->current_ptr && *context->current_ptr != ']' ) {
        skip_whitespace(context);
        JsonValue *value = parse_value(context, error);
        if (!value) return nullptr;  // error out immediately
        elements = add_json_value_node(elements, value);
        num_elements++;
        skip_whitespace(context);
        if (*context->current_ptr == ',' ) {
            // comma is expected delimiter between array elements
            advance(context, 1);  // consume ','
        }
        // context->current_ptr++;
    }

    if (*context->current_ptr != ']' ) {
        char const *msg = "unterminated array";
        record_error(context, error, (int)strlen(msg), msg);
        return nullptr;
    }

    advance(context, 1);  // consume ']'
    JsonValue **element_array = arena_alloc(&arena, sizeof(JsonValue) *  num_elements);
    // copy linked list elements into new array in reverse order so the list maintains the order from the json file.

    for (size_t i = num_elements; i--> 0; ) {
        element_array[i] = elements->value;
        elements = elements->next;
    }
    array->u.array.count = num_elements;
    array->u.array.elements = element_array;

    return array;

}

typedef struct json_object_entry_node_s {
    JsonObjectEntry *object_entry;
    struct json_object_entry_node_s *next;
} JsonObjectEntryNode;

static JsonObjectEntryNode * add_json_object_entry_node(JsonObjectEntryNode * first_node, JsonObjectEntry *object_entry) {
    if (!object_entry) return nullptr;
    JsonObjectEntryNode * new_node = (JsonObjectEntryNode*) arena_alloc(&arena, sizeof(JsonObjectEntryNode) );
    new_node->object_entry = object_entry;
    new_node->next = first_node;
    return new_node;
}


JsonObjectEntry * jsonp_entry_for_key(const JsonValue *json_obj, char const * key) {
    assert(json_obj->type == JSON_OBJECT);

    for (uint32_t i = 0; i < json_obj->u.object.count; ++i) {
        char const * entry_key = json_obj->u.object.entries[i]->key;
        if (strcmp(entry_key, key) == 0 ) {
            return json_obj->u.object.entries[i];
        }
    }

    return nullptr;
}

static JsonValue * parse_object(JsonContext *context, JsonError *error) {
    // we recursively parse elements of this object until we see end-of-object '}' char
    JsonValue *object =  arena_alloc(&arena, sizeof(JsonValue) );

    size_t num_entries = 0;
    object->type = JSON_OBJECT;
    object->u.object.count   = num_entries;
    object->u.object.entries = nullptr;

    JsonObjectEntryNode *entries = nullptr;
    advance(context, 1);  // consume '{'
    skip_whitespace(context);

    while ( *context->current_ptr && *context->current_ptr != '}' ) {
        skip_whitespace(context);
        JsonValue *key = parse_string(context, error);
        if (!key) return nullptr;  // error out immediately
        skip_whitespace(context);

        // need to parse a colon ":" here:
        if (*context->current_ptr != ':' ) {
            char const *msg = "expected name-separator ':'";
            record_error(context, error, (int)strlen(msg), msg);
            return nullptr;
        }
        advance(context, 1);  // consume ':'

        JsonValue *value = parse_value(context, error);
        if (!value) return nullptr;  // error out immediately

        JsonObjectEntry *joe = (JsonObjectEntry*)arena_alloc(&arena, sizeof(JsonObjectEntry));
        if (!joe) {
            fprintf(stderr, "parse_object(): arena alloc failed for JsonObjectEntry *joe\n");
            return nullptr;
        }

        joe->key = key->u.string;
        joe->value = value;
        entries = add_json_object_entry_node(entries, joe);
        num_entries++;

        skip_whitespace(context);
        if (*context->current_ptr == ',' ) {
            // comma is expected delimiter between key:value elements
            advance(context, 1);  // consume ','
        }
    }

    if (*context->current_ptr != '}' ) {
        char const *msg = "unterminated object";
        record_error(context, error, (int)strlen(msg), msg);
        return nullptr;
    }

    advance(context, 1);  // consume '}'
    JsonObjectEntry **entries_array = arena_alloc(&arena, sizeof(JsonObjectEntry) *  num_entries);
    // copy linked list elements into new array in reverse order so the object entries maintain
    // the order from the JSON file.

    for (size_t i = num_entries; i--> 0; ) {
        entries_array[i] = entries->object_entry;
        entries = entries->next;
    }
    object->u.object.count = num_entries;
    object->u.object.entries = entries_array;

    return object;
}



static JsonValue *parse_value(JsonContext *context, JsonError *error) {
    skip_whitespace(context);
    JsonValue *value = nullptr;
    context->parse_start = context->parse_end;
    switch (*context->current_ptr) {
        case 'n': /* Handle null */ {
            value = parse_null(context, error) ;
            break;
        }
        case 't': /* Handle true */ {
            value = parse_true(context, error);
            break;
        }
        case 'f': /* Handle false */ {
            value = parse_false(context, error);
            break;
        }
        case '"': /* Handle string */ {
            value = parse_string(context, error);
            break;
        }
        case '[': /* Handle array */
            value = parse_array(context, error);
            break;
        case '{': /* Handle object */
            value = parse_object(context, error);
            break;
        case '-': case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7': case '8': case '9':
            /* Handle number */ {
                value = parse_number(context,error);
            }
            break;
        default:
            if (error) {
                snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE, "unexpected character:'%c'", *context->current_ptr);
                record_error(context, error, ERROR_MSG_BUFFER_SIZE, error_msg_buffer);
            }
            return nullptr;
    }


    return value;

}

JsonValue *json_parse(const char *json, JsonError *error) {
    if (!json) {
        *error = (JsonError){ .message = "null json string"};
        return nullptr;
    }
    JsonContext context = {.current_ptr = json, .json=json, .line = 1, .column = 1};
    if (json[0] == '\0') {
        *error = (JsonError){.line=1, .column=1, .message = "empty json string"};
        return nullptr;
    }

    JsonValue *value =  parse_value(&context, error);

    return value;
}

char const * json_typename_for_enum(const json_type type) {
    switch (type) {
        case JSON_NULL:
            return "null";
        case JSON_BOOLEAN:
            return "boolean";
        case JSON_NUMBER:
            return "number";
        case JSON_INT:
            return("number(long)");
        case JSON_FLOAT:
            return("number(double)");
        case JSON_STRING:
            return "string";
        case JSON_ARRAY:
            return "array";
        case JSON_OBJECT:
            return "object";
    }
    return "unknown";
}

void json_repr(JsonValue *value);

void json_array_repr(JsonValue *array) {
    if (!array || array->type != JSON_ARRAY) return;
    printf("\n(array[%zd]){\n", array->u.array.count);
    for (size_t i = 0; i < array->u.array.count; ++i) {
        json_repr(array->u.array.elements[i]);
    }
    printf("}\n");


}

void json_object_repr(JsonValue *object) {
    if (!object || object->type != JSON_OBJECT) return;
    printf("\n(object[%zd]){\n", object->u.object.count);
    for (size_t i = 0; i < object->u.object.count; ++i) {
        printf("%s : ",object->u.object.entries[i]->key);
        json_repr(object->u.object.entries[i]->value);
        printf(", \n");
    }
    printf("}\n");

}


void json_repr(JsonValue *value) {
    printf("(JsonValue:%s){ ", json_typename_for_enum(value->type));
    switch (value->type) {
        case JSON_NULL:
            printf("null");
            break;
        case JSON_BOOLEAN:
            if (value->u.boolean) printf("true");
            else printf("false");
            break;
        case JSON_NUMBER:
            printf("%g", value->u.n_number);
            break;
        case JSON_INT:
            printf("%ld", value->u.n_long);
            break;
        case JSON_FLOAT:
            printf("%g", value->u.n_double);
            break;
        case JSON_STRING:
            printf("'%s'", value->u.string);
            break;
        case JSON_ARRAY:
            // printf("[%zd]", value->u.array.count);
            json_array_repr(value);
            break;
        case JSON_OBJECT:
            json_object_repr(value);
            break;
    }

}

void json_value_repr(JsonValue *value) {
    printf("(JsonValue){ type(%d)=%s, value=", value->type, json_typename_for_enum(value->type));
    switch (value->type) {
        case JSON_NULL:
            printf("null");
            break;
        case JSON_BOOLEAN:
            if (value->u.boolean) printf("true");
            else printf("false");
            break;
        case JSON_NUMBER:
            printf("%g", value->u.n_number);
            break;
        case JSON_INT:
            printf("%ld", value->u.n_long);
            break;
        case JSON_FLOAT:
            printf("%g", value->u.n_double);
            break;
        case JSON_STRING:
            printf("'%s'", value->u.string);
            break;
        case JSON_ARRAY:
            // printf("[%zd]", value->u.array.count);
            json_array_repr(value);
            break;
        case JSON_OBJECT:
            printf("{}");
            break;
    }

}


// -----------------------------------------------------------------
//      Pretty-Printer like output
// -----------------------------------------------------------------

void json_array_str(JsonValue *array) {
    if (!array || array->type != JSON_ARRAY) return;
    printf("[ ");
    for (size_t i = 0; i < array->u.array.count; ++i) {
        json_value_str(array->u.array.elements[i]);
        printf(", ");
    }
    printf(" ]");
}

void json_object_str(JsonValue *object) {
    if (!object || object->type != JSON_OBJECT) return;
    printf("{ ");
    for (size_t i = 0; i < object->u.object.count; ++i) {
        printf(" '%s' : ",object->u.object.entries[i]->key);
        json_value_str(object->u.object.entries[i]->value);
        printf(", ");
    }
    printf(" }");
}

void json_value_str(JsonValue *value) {
    switch (value->type) {
        case JSON_NULL:
            printf("null");
            break;
        case JSON_BOOLEAN:
            if (value->u.boolean) printf("true");
            else printf("false");
            break;
        case JSON_NUMBER:
            printf("%g", value->u.n_number);
            break;
        case JSON_INT:
            printf("%ld", value->u.n_long);
            break;
        case JSON_FLOAT:
            printf("%g", value->u.n_double);
            break;
        case JSON_STRING:
            printf("'%s'", value->u.string);
            break;
        case JSON_ARRAY:
            json_array_str(value);
            break;
        case JSON_OBJECT:
            json_object_str(value);
            break;
    }
}

constexpr int COMPILE_SUCCESS = 0;


static bool is_initialized = false;

Error init_regex_patterns(void) {
    RegexPattern *p_list[] = {
        &REGEX_TRUE_PATTERN, &REGEX_FALSE_PATTERN,
        &REGEX_NULL_PATTERN, &REGEX_STRING_PATTERN , &REGEX_NUMBER_PATTERN
    };

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


//// ------------------------------------------------------------
////
////    TESTING
////
//// ------------------------------------------------------------


void test_parse_str(char const * str) {
    JsonError err = {.json = str};
    printf("\nParsing json string '%s': \n", str);
    JsonValue *jval = json_parse(str, &err);
    if (!jval) {
        printf("ERROR : line:%d col:%d start:%d end:%d  %s\n",
            err.line, err.column, err.parse_start, err.parse_end -1, err.message);
    }
    else {
        json_value_str(jval);
        printf("\n");
    }
}

void test_literals() {
    test_parse_str(" null ");
    test_parse_str("true");
    test_parse_str("false");

    test_parse_str("             true");
    test_parse_str("             true           ");

    test_parse_str("true false");
    test_parse_str("falsee [\"list\"]");
    test_parse_str("nul");
    test_parse_str("nulll");
    test_parse_str("nullington");
}

void test_strings( ) {
    test_parse_str(nullptr);
    test_parse_str("");

    test_parse_str("\"\"");
    test_parse_str("\"string\"");


    // test_parse_str("^%$the heck is this mess?");


    test_parse_str(" \"This is a json string followed by a comma\", ");
    test_parse_str(" \"This  \\\"json\\\" string has embedded quotes\" ");


    test_parse_str(" \"This string has no matching closing quote. ");
    test_parse_str(" \"This  \\\"json\\\" string has embedded quotes but no matching closing quote ");


    test_parse_str(" \"This  \\\"json\\\" string has no\nclosing quote , but it has a delimeter and newline");

    test_parse_str(" \"This string has a newline here->\n that is not escaped.  \"");

    test_parse_str(" \"This string has a control character 0x13 here->\\\n that IS escaped.  \"");

    test_parse_str(" \"This string has an escape-n newline here->\\n that IS escaped.  \"");
}


void test_string_escapes(void) {
    test_parse_str("\"\"");

    test_parse_str(" \"Esc-backslash: s\\\\e  \" ");
    test_parse_str(" \"Esc-quote:     s\\\"e  \" ");
    test_parse_str(" \"Esc-Slash:     s\\/e  \" ");
    test_parse_str(" \"Esc-b:          s\\be  \" ");
    test_parse_str(" \"Esc-f:          s\\fe  \" ");
    test_parse_str(" \"Esc-n:          s\\ne  \" ");
    test_parse_str(" \"Esc-r:          s\\re  \" ");
    test_parse_str(" \"Esc-t:          s\\te  \" ");

    test_parse_str("\"Let's test ALL the single char escapes: \\\\ \\\" \\/ \\b \\f \\n \\r \\t\"");


    test_parse_str(" \" backslash           \\ no character \" ");
    test_parse_str(" \" escaped backslash \\\\ valid \" ");


    test_parse_str(" \" backslash-z \\z not valid escape character \" ");
    test_parse_str(" \" backslash-n \\n valid escape character \" ");

    test_parse_str(" \" backslash-u  \\u  invalid  \" ");
    test_parse_str(" \" backslash-uk \\uk invalid  \" ");

    test_parse_str(" \" backslash-ua  \\ua  need 4 hex digits\" ");  // need 4 digits here
    test_parse_str(" \" backslash-uabcd  \\uabcd valid \" ");
    test_parse_str(" \" backslash-uFEF0  \\uFEF0 valid \" ");

    test_parse_str(" \" backslash-uFEF0  \\uFEF00 one too many. Parses as valid, but next character will fail parse \" ");



}

void test_parse_numbers(void) {
    test_parse_str("0");
    test_parse_str("-0");

    test_parse_str("1");
    test_parse_str("-2");
    test_parse_str("3.333");
    test_parse_str("-122.3959");
    test_parse_str("9.99e10");
    test_parse_str("-8.88E-8");

    test_parse_str("-abc"); // should fail
}

void test_parse_arrays(void) {
    test_parse_str("[]");   // empty list is fine by the spec
    test_parse_str("[ null ]");
    test_parse_str("[ null, true, false ]");

    test_parse_str("[ null, true, false, \"string\", 3, 3.333 ]");

    // array of 3 arrays with string elementss
    test_parse_str("[ [\"list1:one\", \"list1:two\"], [\"list2:one\", \"list2:two\"], [\"list3:one\", \"list3:two\"]]");

    test_parse_str( "["\
  "{ \"id\": 0, \"name\":  \"ELON ANDROID\", \"ff\": 1},"  \
  "{ \"id\": 1, \"name\":  \"ELON ANDROID\", \"ff\": 5},"  \
  "{ \"id\": 2, \"name\":  \"ELON ANDROID\", \"ff\": 10}," \
  "{ \"id\": 3, \"name\":  \"ELON ANDROID\", \"ff\": 15}," \
  "{ \"id\": 4, \"name\":  \"ELON ANDROID\", \"ff\": 20}"  \
    "]" );
}

void test_parse_objects(void ) {
    test_parse_str("{}");   // empty object is fine by the spec

    test_parse_str("{ \"foo\": null }");

    test_parse_str("{ \"one\": \"one\", \"two\" : 2, \"three\" : 3.33 }   ");

    test_parse_str("{ \"one\": \"one\", \"two\" : 2, \"three\" : 3.33, \"null\" : null, \"true\":true, \"false\":false }");

    test_parse_str(" [{ \"name\": \"jelly bowl\", \"ff\": 5}, {\"name\": \"werewolf\", \"ff\": 10 }]");
}

#ifdef JSON_PARSER_MAIN
int main( ) {
    // test string_builder
    Error err = jsonp_init();
    if (err.err) {
        err_print(err);
        return err.reported_err;
    }

    test_literals();

    test_strings();
    test_parse_numbers();
    test_string_escapes();

    test_parse_arrays();
    test_parse_objects();

    // temp
    StringBuilder sb = {};
    StringBuilder *sb_ptr = &sb;
    sb_init( sb_ptr, 0, "This is a string to test.");

    sb_destroy(sb_ptr);
    jsonp_destroy();

    // Another Crappy Json Parser = ACJP

}
#endif