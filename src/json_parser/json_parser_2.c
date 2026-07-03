//  json_parser_2.c
// Created by Rob Ross on 7/2/26.
//
// Migrating and reworking the existing json_parser.c here, which will replace it when completed


#include "roblib/json_parser.h"
#include <ctype.h>
#include <stdio.h>

#include <stdlib.h>
#include <string.h>

typedef struct json_context_t {
    const char *current_ptr;  // The current text being parsed, advances through the JSON text in the json member
    const char *json; // full original JSON text string
    int current_index; // the index of the character the lexer is scanning
    int line;
    int column;
    int parse_start;
    int parse_end;
} JsonContext;

constexpr uint32_t ERROR_MSG_BUFFER_SIZE = 1023;
static char error_msg_buffer[ERROR_MSG_BUFFER_SIZE + 1] = {};


// -----------------------------------------------------------------
//      Forward Declarations
// -----------------------------------------------------------------
static JsonValue *parse_value(JsonContext *context, JsonError *error, Arena *arena );




/**
    *ws = *(
    %x20 /  ; Space
    %x09 /  ; Horizontal tab
    %x0A /  ; Line feed or New line
    %x0D )  ; Carriage return
*/
static char const * const WHITE_SPACE_CHARS = "\x20\x09\x0A\x0D";

static void skip_whitespace(JsonContext *context) {
    //todo (rob) verify that isspace uses the same space characters as the spec (see WHITE_SPACE_CHARS0
    while (*context->current_ptr && isspace(*context->current_ptr)) {
        if (*context->current_ptr == '\n') {
            context->line++;
            context->column = 1;
        } else {
            context->column++;
        }
        context->current_index++;
        context->current_ptr++;
    }
}

// advance the parser state based on the current parse window
static void advance(JsonContext *context, const int char_count) {
    context->current_ptr    += char_count;
    context->current_index  += char_count;
    context->column         += char_count;
}

static void record_error(
    const JsonContext *context, JsonError *error, enum json_error_type err_type, const char *msg) {

    error->message = msg;
    error->json = context->json;
    error->err_type = err_type;
    error->first_bad_char = context->current_index;
    error->line   = context->line;
    error->column = context->column;
    error->parse_start = context->parse_start;
    error->parse_end = context->parse_end;
}


static JsonValue * parse_object(JsonContext *context, JsonError *error, Arena *arena ) {
    // we recursively parse members of this object until we see end-of-object '}' char or run out of text
    JsonValue *object =  arena_alloc(arena, sizeof(JsonValue) );

    return object;
}

typedef struct json_value_node_s {
    JsonValue *value;
    struct json_value_node_s *next;
} JsonValueNode;

// add a node to the linked list headed by first_node.
// adds nodes to the front of the linked list. If first_node is null, it will be the head node of a new linked list.
// Returns the new first_node.
static JsonValueNode * add_json_value_node(JsonValueNode * first_node, JsonValue *value, Arena *arena ) {
    if (!value) return nullptr;
    JsonValueNode * new_node = (JsonValueNode*) arena_alloc(arena, sizeof(JsonValueNode) );
    new_node->value = value;
    new_node->next = first_node;
    return new_node;
}


static JsonValue * parse_array(JsonContext *context, JsonError *error, Arena *arena ) {
    // we recursively parse elements of this array until we see end-of-array ']' char or run out of text
    JsonValue *array =  arena_alloc(arena, sizeof(JsonValue) );

    size_t num_elements = 0;
    array->type = JSON_ARRAY;
    array->u.array.count = num_elements;
    array->u.array.elements = nullptr;

    JsonValueNode *elements = nullptr;
    advance(context, 1);  // consume '['
    skip_whitespace(context);
    //parse first element, then parse comma, elements until ']' or EOF
    if ( *context->current_ptr && *context->current_ptr != ']') {
        JsonValue *value = parse_value(context, error, arena);
        if (!value) {
            context->parse_start = context->current_index;
            context->parse_end = context->current_index;
            return nullptr;  // error-out immediately
        }
        elements = add_json_value_node(elements, value, arena);
        num_elements++;
        skip_whitespace(context);
    }

    while ( *context->current_ptr && *context->current_ptr != ']' ) {
        skip_whitespace(context);
        //expect ','
        if (*context->current_ptr != ',' ) {
            context->parse_start = context->current_index;
            context->parse_end = context->current_index;
            record_error(context, error, JSON_ERR_MISSING_COMMA, "expected comma");
            return nullptr;
        }
        // comma is expected delimiter between array elements
        advance(context, 1);  // consume ','
        skip_whitespace(context);

        JsonValue *value = parse_value(context, error, arena);
        // if (!value) return nullptr;  // error out immediately
        if (!value) {
            break;
        }
        elements = add_json_value_node(elements, value, arena);
        num_elements++;
    }
    skip_whitespace(context);

    if (*context->current_ptr != ']' ) {
        char const *msg = "unterminated array";
        context->parse_end = context->current_index;
        record_error(context, error, JSON_ERR_UNTERMINATED_ARRAY, msg);
        return nullptr;
    }
    advance(context, 1);  // consume ']'
    context->parse_end = context->current_index;
    JsonValue **element_array = arena_alloc(arena, sizeof(JsonValue) *  num_elements);
    // copy linked list elements into new array in reverse order so the list maintains the order from the JSON file.

    for (size_t i = num_elements; i--> 0; ) {
        element_array[i] = elements->value;
        elements = elements->next;
    }
    array->u.array.count = num_elements;
    array->u.array.elements = element_array;

    return array;
}

static JsonValue * parse_string(JsonContext *context, JsonError *error, Arena *arena ) {

    JsonValue *value = nullptr;


    return value;
}

// RSL: "Regex Start of Line"
#define RSL      "^"
#define WS       "[\x20\x09\x0A\x0D]"
#define WS_star  "[\x20\x09\x0A\x0D]*"
static const char * const REGEX_NUMBER_STR = RSL "(-?(0|[1-9])[0-9]*(\\.[0-9]+)?([eE][-+]?[0-9]+)?)" WS_star;
static regex_t REGEX_NUMBER_PATTERN;
constexpr int MATCH_FOUND = 0;

static JsonValue * parse_number(JsonContext *context, JsonError *error, Arena *arena ) {
    JsonValue *value = nullptr;

    constexpr size_t max_groups = 4;
    // The first element (0) is the entire match, subsequent elements are capture groups
    regmatch_t pmatch[max_groups] = {}; // Assuming max_groups - 1 capture groups + full match
    const int result = regexec( &REGEX_NUMBER_PATTERN, context->current_ptr, max_groups, pmatch, 0);
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
        if (context->current_ptr[i]=='.') {
            is_double = true;
            break;
        }
    }

    value = arena_alloc(arena, sizeof(JsonValue) );
    value->type = JSON_NUMBER;
    if (is_double) {
        value->type = JSON_FLOAT;
        value->u.n_double = strtod(context->current_ptr, nullptr);
    } else {
        value->type = JSON_INT;
        value->u.n_long = strtol(context->current_ptr, nullptr, 10);

    }
    advance(context, match_len);
    context->parse_end = context->current_index;
    return value;
}

inline static JsonValue *  parse_literal_impl(  JsonContext *context,
                                                JsonError *error,
                                                JsonValue *literal,
                                                char const *key_word ) {

    context->parse_start = context->current_index;
    int current_index = context->current_index;
    char const * keyword_ptr = key_word;
    char const * text_ptr = context->current_ptr;
    enum json_error_type err_type = JSON_ERR_NONE;

    while ( *keyword_ptr != '\0' && *text_ptr != '\0') {
        if (*text_ptr == '\n') {
            context->line++;
            context->column = 0;
        }
        if (*keyword_ptr != *text_ptr) {
            err_type = JSON_ERR_UNEXPECTED_TEXT;
            snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE,
                "unexpected character:'%c', expected '%s'", *text_ptr, key_word);
            break;
        }
        context->column++;
        current_index++;
        keyword_ptr++;
        text_ptr++;
    }

    context->current_ptr = text_ptr;
    context->current_index = current_index;
    context->parse_end = current_index;

    if ( err_type == JSON_ERR_NONE && *keyword_ptr != '\0') {
        // unexpected end of text
        err_type = JSON_ERR_UNEXPECTED_EOF;
        snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE, "unexpected EOF, expected '%s'", key_word);
    }
    if (err_type != JSON_ERR_NONE) {
        record_error(context, error, err_type, error_msg_buffer);
        return nullptr;
    }

    return literal;
}


// todo (rob) these JsonValues need to be const
constexpr size_t JSON_KEYWORD_NULL_LEN = 4;
constexpr char   JSON_KEYWORD_NULL[JSON_KEYWORD_NULL_LEN + 1] = "null";
static JsonValue JSON_NULL_VALUE = { .type = JSON_NULL, .u.string = JSON_KEYWORD_NULL};

constexpr size_t JSON_KEYWORD_TRUE_LEN = 4;
constexpr char   JSON_KEYWORD_TRUE[JSON_KEYWORD_TRUE_LEN + 1] = "true";
static JsonValue JSON_TRUE_VALUE = { .type = JSON_BOOLEAN, .u.boolean = true};

constexpr size_t JSON_KEYWORD_FALSE_LEN = 5;
constexpr char   JSON_KEYWORD_FALSE[JSON_KEYWORD_FALSE_LEN + 1] = "false";
static JsonValue JSON_FALSE_VALUE = { .type = JSON_BOOLEAN, .u.boolean = false};

static JsonValue *  parse_true(JsonContext *context, JsonError *error ) {
    return parse_literal_impl(context, error, &JSON_TRUE_VALUE, JSON_KEYWORD_TRUE );
}

static JsonValue *  parse_false(JsonContext *context, JsonError *error ) {
    return parse_literal_impl(context, error, &JSON_FALSE_VALUE, JSON_KEYWORD_FALSE );
}

static JsonValue *  parse_null(JsonContext *context, JsonError *error ) {
    return parse_literal_impl(context, error, &JSON_NULL_VALUE, JSON_KEYWORD_NULL );
}

static JsonValue *parse_value(JsonContext *context, JsonError *error, Arena *arena ) {
    skip_whitespace(context);
    context->parse_start = context->current_index;
    JsonValue *value = nullptr;
    switch (*context->current_ptr) {
        case '{': /* Handle object */
            value = parse_object(context, error, arena);
            break;
        case '[': /* Handle array */
            value = parse_array(context, error, arena);
            break;
        case '"': /* Handle string */
            value = parse_string(context, error, arena);
            break;
        case '-': case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7': case '8': case '9':
            /* Handle number */
            value = parse_number(context,error, arena);
            break;
        case 't': /* Handle true */
            value = parse_true(context, error);
            break;
        case 'f': /* Handle false */
            value = parse_false(context, error);
            break;
        case 'n': /* Handle null */
            value = parse_null(context, error) ;
            break;

        default:
            if (error) {
                snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE, "unexpected character:'%c'", *context->current_ptr);
                record_error(context, error, JSON_ERR_UNEXPECTED_TEXT, error_msg_buffer);
            }
            break;

    }

    return value;
}



JsonValue *json_parse(const char *json, JsonError *error, Arena *arena) {

    if (!json) {
        *error = (JsonError){ .json=json, .message = "null json text", .err_type = JSON_ERR_NULL_TEXT};
        return nullptr;
    }

    JsonContext context = {.current_ptr = json, .json=json};
    skip_whitespace(&context);
    if (json[0] == '\0') {
        *error = (JsonError){.json=json, .message = "empty json text", .err_type = JSON_ERR_EMPTY_TEXT};
        return nullptr;
    }

    //A JSON value MUST be an object, array, number, or string, or one of the following three literal names:
    //  false, null, true

    error->err_type = JSON_ERR_NONE;
    JsonValue *value =  parse_value(&context, error, arena);
    if ( error->err_type != JSON_ERR_NONE) {
        return nullptr;
    }

    skip_whitespace(&context);

    if (*context.current_ptr != '\0') {
        //we parsed the root value, but there is still text remaining in the JSON text, which is an error
        record_error(&context, error, JSON_ERR_UNEXPECTED_TEXT, "unexpected json text after parsing value");
        return nullptr;
    }


    return value;
}

constexpr int COMPILE_SUCCESS = 0;
static bool is_initialized = false;

Error jsonp_init() {
    int reti = regcomp(&REGEX_NUMBER_PATTERN, REGEX_NUMBER_STR, REG_EXTENDED);
    if ( reti != COMPILE_SUCCESS) {
        char msgbuf[100];
        regerror(reti, &REGEX_NUMBER_PATTERN, msgbuf, sizeof(msgbuf));
        fprintf(stderr, "Regex compilation failed for '%s': %s\n", REGEX_NUMBER_STR, msgbuf);
        Error result =  (Error){ .err = true, .reported_err = reti, .err_obj = (void*)REGEX_NUMBER_STR };
        strncpy(result.msg, msgbuf, sizeof msgbuf);
        result.msg[ sizeof msgbuf - 1 ] = '\0';
        return result; // Exit early on compilation error
    }

    is_initialized = true;
    return (Error){};
}

void jsonp_destroy(void) {
    regfree(&REGEX_NUMBER_PATTERN);
    is_initialized = false;
}

// -----------------------------------------------------------------
//      Pretty-Printer like output
// -----------------------------------------------------------------

void json_array_str(JsonValue *array) {
    if (!array || array->type != JSON_ARRAY) return;
    if (array->u.array.count == 0 ) {
        printf("[ ]");
        return;
    }
    printf("[ ");
    json_value_str(array->u.array.elements[0]);
    for (size_t i = 1; i < array->u.array.count; ++i) {
        printf(", ");
        json_value_str(array->u.array.elements[i]);
    }
    printf(" ]");
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
            // json_object_str(value);
            break;
    }
}

//// ------------------------------------------------------------
////
////    TESTING
////
//// ------------------------------------------------------------


void test_parse_str(char const * str) {
    Error init_err = jsonp_init();
    Arena arena = {};
    ArenaErrResult aer = arena_create_arena( &arena, ONE_MIBIBYTE * 100);
    if ( aer.err ) {
        printf("arena_create_arena failed with %d, %s\n", aer.reported_err, aer.msg);
    }
    JsonError err = {.json = str};
    printf("\nParsing json string '%s': \n", str);
    JsonValue *jval = json_parse(str, &err, &arena);
    if (!jval) {
        printf("ERROR %d: first_bad_char:%d, line:%d col:%d start:%d end:%d  %s\n",
           err.err_type, err.first_bad_char,  err.line, err.column, err.parse_start, err.parse_end, err.message);
    }
    else {
        json_value_str(jval);
        printf("\n");
    }

    arena_destroy_arena(&arena);
    jsonp_destroy();
}

void test_null_parse(void) {
    test_parse_str("null");
    test_parse_str(" null ");
    test_parse_str("nul");
    test_parse_str("nu");
    test_parse_str("n");

    test_parse_str("number");
    test_parse_str("next");

    test_parse_str("nulll");
}

void test_true_parse(void) {
    test_parse_str("true");
    test_parse_str(" true ");
    test_parse_str(" truetrue ");
    test_parse_str(" true true");
    test_parse_str(" tr true");
}

void test_false_parse(void) {
    test_parse_str("false");
    test_parse_str(" false ");
    test_parse_str(" falsefalse ");
    test_parse_str(" false false");
    test_parse_str(" fals ");
}

void test_number_parse(void) {
    test_parse_str("0");
    test_parse_str("1");
    test_parse_str("1.1");
    test_parse_str("-3.3");
    test_parse_str("-3.3e24");
    test_parse_str("5.3e-24");
    test_parse_str("5.67 moo");

}

void test_array_parse(void) {
    // test_parse_str("[]");
    // test_parse_str("[1]");
    // test_parse_str("[1, 2]");
    // test_parse_str("[1 2]");
    //
    // test_parse_str("[1, 2, true, false, null, 3.33, 4e20]");
    //
    // test_parse_str("[ 1, 2]]");

    // test_parse_str("[ 1, ");
//[ [1,2,3], [4,5,6], [true,false,null] ]
    test_parse_str("[[1,2 ] ] ");
    test_parse_str("[[1 ], [2 ] ] ");
    // test_parse_str("[[1,2] ] ");
    // test_parse_str("[[1,2],[3,4]] ");
    // test_parse_str("[ [1,2],[3,4]] ");
    // test_parse_str("[ [1,2], [3,4]] ");
    // test_parse_str("[ [1,2], [3,4] ] ");
    //
    // test_parse_str("[ [1,2,3], [4,5,6], [true,false,null] ] ");

}

#ifdef JSON_PARSER_2_MAIN
int main( ) {

    // test_null_parse();
    // test_true_parse();
    // test_false_parse();
    // test_number_parse();
    test_array_parse();
}
#endif
