//  json_parser.c
// Created by Rob Ross on 7/2/26.
//
// JSONP v0.1.0


#include "roblib/json_parser.h"
#include <ctype.h>

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdatomic.h>

// // POSIX.1-2008 locale functions.
// // Modern glibc (Linux) uses <locale.h>, while macOS/BSD often requires <xlocale.h>.
// #ifndef _WIN32
// #include <xlocale.h>
// #endif

#include <assert.h>

#include "roblib/string_builder.h"
#include "roblib/unicode_tools.h"

/*
 *  todo (rob) tasks:
 *  1. pretty printer
 *  2. file writer to save JSON to disk
 *  3. file parser, to parse a json file document instead of a string
 *  4. implement current config flags behaviors in code

 *  6. add ability to set flags on context
 *  7. add ability to change depth on context
 *  8. add ability to change whitespace on context
 *  9. documentation
 *  10. testing.
 *  11. add warnings if int promoted to float, or float over/underflows
 */



// -----------------------------------------------------------------
//      BOM CONSTANTS
// -----------------------------------------------------------------

static char const * const BOM_UTF8     = "\xEF\xBB\xBF";
static char const * const BOM_UTF16_BE = "\xFE\xFF";
static char const * const BOM_UTF16_LE = "\xFF\xFE";
static char const * const BOM_UTF32_BE = "\x00\x00\xFE\xFF";
static char const * const BOM_UTF32_LE = "\xFF\xFE\x00\x00";


typedef struct json_context_s {
    const char     *current_ptr;   // The current text being parsed, advances through the JSON text in the json member
    const char     *json_text;     // full original JSON text string
    uint32_t       current_index; // the index of the character the lexer is scanning
    uint32_t       line;
    uint32_t       column;
    uint32_t       parse_start;
    uint32_t       parse_end;
    uint32_t       depth_current;
    uint32_t       depth_max;
    jp_bitset_t    config_flags;
    char           decimal_separator; // used when converting float values
    char           whitespace_chars[16];
    bool           ws_table[256];
    char           error_msg[ERROR_MSG_BUFFER_SIZE + 1];
} JsonContext;

// -----------------------------------------------------------------
//      READER FUNCTIONS
// -----------------------------------------------------------------

typedef size_t (*read_fn)(
    void *context,
    unsigned char *buffer,
    size_t max_bytes);

typedef struct
{
    read_fn read;
    void *context;
} Input;

size_t file_read(...);
size_t socket_read(...);
size_t memory_read(...);
size_t http_read(...);




// -----------------------------------------------------------------
//      Forward Declarations
// -----------------------------------------------------------------
static JsonValue * pvt_parse_value(JsonContext *context, JsonParseError *error, Arena *arena );
static JsonValue * pvt_parse_string(JsonContext *context, JsonParseError *error, Arena *arena );
static void pvt_init_context_whitespace_table(JsonContext *context);
static char pvt_peek_char(JsonContext const *context);
static bool pvt_starts_with_bom(const char json_text[static 1], const uint32_t n_bytes, char const bom_bytes[static n_bytes]);
static bool pvt_is_rejected_due_to_bom(JsonContext *context, const char *json_text, JsonParseError *error);




// -----------------------------------------------------------------
//      CONFIG FLAGS
// -----------------------------------------------------------------

// Using _Atomic ensures that setting a flag in one thread and reading it
// to initialize a context in another thread is thread-safe and visible.
static _Atomic(jp_bitset_t) json_config_flags = 0;


bool jsonp_is_context_config_flag_set(const JsonContext *context, const JsonConfigFlag flag) {
    return context->config_flags & ( 1 << flag) ;
}

void jsonp_set_context_config_flag( JsonContext *context, JsonConfigFlag flag) {
    context->config_flags |= (1 << flag);
}

void jsonp_clear_context_config_flag( JsonContext *context, JsonConfigFlag flag) {
    context->config_flags &= ~(1 << flag);
}

jp_bitset_t jsonp_make_config_flag_bitset(const uint32_t flag_count, JsonConfigFlag const *flag) {
    jp_bitset_t set_flags = 0;
    for (uint32_t i = 0; i < flag_count; ++i) {
        set_flags |= ( 1 << flag[i]);
    }
    return set_flags;
}



// -----------------------------------------------------------------
//      WHITE SPACE
// -----------------------------------------------------------------

static _Atomic(const char *) pvt_whitespace_chars = nullptr;

void jsonp_set_context_whitespace_chars( JsonContext *context, const char  *whitespace_chars ) {
    if (!whitespace_chars) return;
    snprintf(context->whitespace_chars, sizeof(context->whitespace_chars), "%s", whitespace_chars);
    pvt_init_context_whitespace_table(context);
}

static inline bool pvt_is_json_whitespace(JsonContext *context, const unsigned char c) {
    return context->ws_table[c];
}

static void pvt_skip_whitespace(JsonContext *context) {
    while ( pvt_is_json_whitespace(context, (const unsigned char)pvt_peek_char(context)) ) {
        if (pvt_peek_char(context) == '\n') {
            context->line++;
            context->column = 0;
        } else {
            context->column++;
        }
        context->current_index++;
        context->current_ptr++;
    }
}

// -----------------------------------------------------------------
//      NESTING DEPTH
// -----------------------------------------------------------------

static _Atomic(uint32_t) pvt_depth_max = JSON_DEPTH_MAX_DEFAULT;

void jsonp_set_context_max_depth(JsonContext *context, uint32_t max_depth){
    context->depth_max = max_depth;
}

// advance the parser state based on the current parse window
static void pvt_advance(JsonContext *context, const uint32_t char_count) {
    context->current_ptr    += char_count;
    context->current_index  += char_count;
    context->column         += char_count;
}

static char pvt_peek_char(JsonContext const *context) {
    return *context->current_ptr;
}

// todo (rob) temp during refactor, this could error out if buffer is at EOF
static char pvt_peek_next_char(JsonContext const *context) {
    return context->current_ptr[1];
}

// Writes to the argument buffer.
// If `c` is a control character ( < 0x20 or > 0x7E) it will format `c` as a hex byte as 0xXX.
// Otherwise, formats `c` as a char.
static void pvt_format_error_message_char(
    const uint32_t buffer_size, char msg_buffer[static buffer_size], const char c ) {

    const unsigned char u_char  = (unsigned char)c;
    if ( u_char == 0x00 ) {
        snprintf(msg_buffer, buffer_size, "NUL");
    } else if ( u_char < 0x20 || u_char > 0x7E) {
        // display non-ascii and control chars as a hex escape.
        snprintf(msg_buffer, buffer_size,
            "0x%.2X", u_char );
    } else {
        snprintf(msg_buffer, buffer_size,
            "'%c'", u_char );
    }
}

static void pvt_record_error(
    const JsonContext *context, JsonParseError *error, const enum json_error_type_e err_type, const char *msg) {

    if (error->message != msg ) {
        strncpy(error->message, msg, ERROR_MSG_BUFFER_SIZE);
    }
    error->json = context->json_text;
    error->err_type = err_type;
    error->first_bad_char = context->current_index;
    error->line   = context->line;
    error->column = context->column;
    error->parse_start = context->parse_start;
    error->parse_end = context->parse_end;
}

static void pvt_record_missing_comma_error(JsonContext *context, JsonParseError *error) {
    int written = snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "expected comma, got ");
    if (written > 0) {
        pvt_format_error_message_char(ERROR_MSG_BUFFER_SIZE - written,
            error->message + written, pvt_peek_char(context) );
    }
    context->parse_end = context->current_index;
    pvt_record_error(context, error, JSON_ERR_MISSING_COMMA, error->message);
}


static bool pvt_max_depth_exceeded(JsonContext *context, JsonParseError *error) {
    if (context->depth_current > context->depth_max) {
        snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
            "max nested depth of %u exceeded", context->depth_max);
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_MAX_NESTED_DEPTH_EXCEEDED, error->message);
        return true;
    }
    return false;
}


typedef struct json_object_entry_node_s {
    JsonObjectEntry *object_entry;
    struct json_object_entry_node_s *next;
} JsonObjectEntryNode;

static JsonObjectEntryNode * pvt_add_json_object_entry_node(JsonObjectEntryNode * first_node, JsonObjectEntry *object_entry, Arena *arena ) {
    if (!object_entry) return nullptr;
    JsonObjectEntryNode * new_node = (JsonObjectEntryNode*) arena_alloc(arena, sizeof(JsonObjectEntryNode) );
    new_node->object_entry = object_entry;
    new_node->next = first_node;
    return new_node;
}


JsonObjectEntry * jsonp_entry_for_key(const JsonValue *json_obj, char const * key) {
    for (uint32_t i = 0; i < json_obj->u.object.count; ++i) {
        char const * entry_key = json_obj->u.object.entries[i]->key;
        if (strcmp(entry_key, key) == 0 ) {
            return json_obj->u.object.entries[i];
        }
    }
    return nullptr;
}


//// ------------------------------------------------------------
////
////    Parsing Functions
////
//// ------------------------------------------------------------

static JsonObjectEntry * pvt_parse_one_entry(JsonContext *context, JsonParseError *error, Arena *arena) {
    pvt_skip_whitespace(context);
    context->parse_start = context->current_index;  // need this here since we aren't calling pvt_parse_value()
    // if (pvt_peek_char(context) == '\0') {
    //     context->parse_end = context->current_index;
    //     snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "expected object key, got EOF");
    //     pvt_record_error(context, error, JSON_ERR_MISSING_OBJECT_KEY, error->message);
    //     return nullptr;
    // }
    if (pvt_peek_char(context) != '"') {
        int written = snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "expected object key, got ");
        if (written > 0) {
            pvt_format_error_message_char(ERROR_MSG_BUFFER_SIZE - written,
                error->message + written, pvt_peek_char(context) );
        }

        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_MISSING_OBJECT_KEY, error->message);
        return nullptr;
    }

    JsonValue *key = pvt_parse_string(context, error, arena);
    if (!key) {
        // pvt_parse_string() will have filled out error struct
        return nullptr;
    }
    pvt_skip_whitespace(context);

    // need to parse a colon ":" here:
    if (pvt_peek_char(context) != ':' ) {
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_MISSING_COLON, "expected name-separator ':'");
        return nullptr;
    }
    pvt_advance(context, 1);  // consume ':'
    pvt_skip_whitespace(context);
    if (pvt_peek_char(context) == '\0') {
        // snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "unexpected EOF, expected object value");

        int written =  snprintf(error->message, ERROR_MSG_BUFFER_SIZE,"expected object value for key '%s', got ", key->u.string);
        if (written > 0) {
            pvt_format_error_message_char(ERROR_MSG_BUFFER_SIZE - written,
                error->message + written, pvt_peek_char(context) );
        }


        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_MISSING_OBJECT_VALUE, error->message);
        return nullptr;
    }

    JsonValue *value = pvt_parse_value(context, error, arena);
    if (!value) {
        // error-out immediately
        return nullptr;
    }
    JsonObjectEntry *joe = (JsonObjectEntry*)arena_alloc(arena, sizeof(JsonObjectEntry));
    if (!joe) {
        char const *const msg = "pvt_parse_one_entry(): arena alloc failed for JsonObjectEntry *joe\n";
        fprintf(stderr, msg);
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_OUT_OF_MEMORY, msg);
        return nullptr;
    }

    joe->key = key->u.string;
    joe->value = value;
    return joe;
}

static JsonValue * pvt_parse_object(JsonContext *context, JsonParseError *error, Arena *arena ) {
    // we recursively parse members of this object until we see end-of-object '}' char or run out of text
    // todo refactor to use stack DS and iterative algorithm instead of recursion
    context->depth_current++;
    if ( pvt_max_depth_exceeded(context, error)) {
        context->depth_current--;
        return nullptr;  // error-out immediately
    }

    JsonValue *object =  arena_alloc(arena, sizeof(JsonValue) );

    size_t num_entries = 0;
    object->type = JSON_OBJECT;
    object->u.object.count   = num_entries;
    object->u.object.entries = nullptr;

    JsonObjectEntryNode *entries = nullptr;
    pvt_advance(context, 1);  // consume '{'
    pvt_skip_whitespace(context);

    //parse first entry, then parse (comma, element)s until '}' or EOF
    if ( pvt_peek_char(context) && pvt_peek_char(context) != '}') {
        JsonObjectEntry *joe = pvt_parse_one_entry(context, error, arena);
        if (!joe) {
            context->depth_current--;
            return nullptr;  // error report filled by previous call
        }
        entries = pvt_add_json_object_entry_node(entries, joe, arena);
        num_entries++;
        pvt_skip_whitespace(context);
    }


    while ( pvt_peek_char(context) && pvt_peek_char(context) != '}' ) {
        pvt_skip_whitespace(context);
        // expect ','
        if (pvt_peek_char(context) != ',' ) {
            pvt_record_missing_comma_error(context, error);
            context->depth_current--;
            return nullptr;
        }
        // comma is expected delimiter between object entries
        uint32_t comma_index = context->current_index; // save comma position for error reporting below
        pvt_advance(context, 1);  // consume ','
        pvt_skip_whitespace(context);

        if (pvt_peek_char(context) == '}') {
            // we had a comma without another entry
            if (jsonp_is_context_config_flag_set(context, JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_OBJECTS)) {
                // this allowed
                break;
            }
            // this is an error
            context->parse_end = context->current_index;
            context->current_index = comma_index;
            context->parse_start = context->current_index;
            pvt_record_error(context, error, JSON_ERR_TRAILING_COMMA_NOT_ALLOWED, "trailing comma not allowed in object entry list");
            context->depth_current--;
            return nullptr;
        }

        JsonObjectEntry *joe = pvt_parse_one_entry(context, error, arena);
        if (!joe) {
            context->depth_current--;
            return nullptr;
        }
        entries = pvt_add_json_object_entry_node(entries, joe, arena);
        num_entries++;
        pvt_skip_whitespace(context);
    }

    if (pvt_peek_char(context) != '}' ) {
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_UNTERMINATED_OBJECT, "missing closing brace '}'");
        context->depth_current--;
        return nullptr;
    }

    pvt_advance(context, 1);  // consume '}'
    JsonObjectEntry **entries_array = arena_alloc(arena, sizeof(JsonObjectEntry) *  num_entries);
    // copy linked list elements into new array in reverse order so the object entries maintain
    // the order from the JSON file.

    for (size_t i = num_entries; i--> 0; ) {
        entries_array[i] = entries->object_entry;
        entries = entries->next;
    }
    object->u.object.count = num_entries;
    object->u.object.entries = entries_array;

    context->depth_current--;
    return object;
}

typedef struct json_value_node_s {
    JsonValue *value;
    struct json_value_node_s *next;
} JsonValueNode;

// add a node to the linked list headed by first_node.
// adds nodes to the front of the linked list. If first_node is null, it will be the head node of a new linked list.
// Returns the new first_node.
static JsonValueNode * pvt_add_json_value_node(JsonValueNode * first_node, JsonValue *value, Arena *arena ) {
    if (!value) return nullptr;
    JsonValueNode * new_node = (JsonValueNode*) arena_alloc(arena, sizeof(JsonValueNode) );
    new_node->value = value;
    new_node->next = first_node;
    return new_node;
}


static JsonValue * pvt_parse_array(JsonContext *context, JsonParseError *error, Arena *arena) {
    // we recursively parse elements of this array until we see end-of-array ']' char or run out of text
    // todo refactor to use stack data structure and iterative algorithm instead of recursion
    context->depth_current++;
    if ( pvt_max_depth_exceeded(context, error)) {
        context->depth_current--;
        return nullptr;  // error-out immediately
    }
    JsonValue *array =  arena_alloc(arena, sizeof(JsonValue) );

    size_t num_elements = 0;
    array->type = JSON_ARRAY;
    array->u.array.count = num_elements;
    array->u.array.elements = nullptr;

    JsonValueNode *elements = nullptr;
    pvt_advance(context, 1);  // consume '['
    pvt_skip_whitespace(context);

    //parse first element, then parse (comma, element)s until ']' or EOF
    if ( pvt_peek_char(context) && pvt_peek_char(context) != ']') {
        JsonValue *value = pvt_parse_value(context, error, arena);
        if (!value) {
            // context->parse_end = context->current_index;
            if (error->err_type == JSON_ERR_UNEXPECTED_TEXT) {
                // use an error code more specific to the parsing context.
                // The error message still contains the details of the error condition
                error->err_type = JSON_ERR_MISSING_ARRAY_ELEMENT;
            }
            context->depth_current--;
            return nullptr;  // error-out immediately
        }
        elements = pvt_add_json_value_node(elements, value, arena);
        num_elements++;
        pvt_skip_whitespace(context);
    }

    while ( pvt_peek_char(context) && pvt_peek_char(context) != ']' ) {
        pvt_skip_whitespace(context);
        //expect ','
        if (pvt_peek_char(context) != ',' ) {
            pvt_record_missing_comma_error(context, error);
            context->depth_current--;
            return nullptr;
        }
        // comma is expected delimiter between array elements
        uint32_t comma_index = context->current_index; // save for error reporting below
        pvt_advance(context, 1);  // consume ','
        pvt_skip_whitespace(context);

        if (pvt_peek_char(context) == ']') {
            // we have a comma without another value
            if (jsonp_is_context_config_flag_set(context, JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_ARRAYS)) {
                // this allowed
                break;
            }
            // this is an error
            context->parse_end = context->current_index;
            context->current_index = comma_index;
            context->parse_start = context->current_index;

            pvt_record_error(context, error, JSON_ERR_TRAILING_COMMA_NOT_ALLOWED, "trailing comma not allowed in array element list");
            context->depth_current--;
            return nullptr;
        }

        JsonValue *value = pvt_parse_value(context, error, arena);
        if (!value) {
            if (error->err_type == JSON_ERR_UNEXPECTED_TEXT) {
                // use an error code more specific to the parsing context.
                // The error message still contains the details of the error condition
                error->err_type = JSON_ERR_MISSING_ARRAY_ELEMENT;
            }
            context->depth_current--;
            return nullptr;
        }
        elements = pvt_add_json_value_node(elements, value, arena);
        num_elements++;
        pvt_skip_whitespace(context);
    }
    // pvt_skip_whitespace(context);

    if (pvt_peek_char(context) != ']' ) {
        char const *msg = "missing closing bracket ']'";
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_UNTERMINATED_ARRAY, msg);
        context->depth_current--;
        return nullptr;
    }
    pvt_advance(context, 1);  // consume ']'
    pvt_skip_whitespace(context);
    JsonValue **element_array = arena_alloc(arena, sizeof(JsonValue) *  num_elements);
    // copy linked list elements into new array in reverse order so the list maintains the order from the JSON file.

    for (size_t i = num_elements; i--> 0; ) {
        element_array[i] = elements->value;
        elements = elements->next;
    }
    array->u.array.count = num_elements;
    array->u.array.elements = element_array;

    context->depth_current--;
    return array;
}


static int pvt_hex_to_dec(const unsigned char hex)
{
    if (hex >= '0' && hex <= '9')
        return hex - '0';
    switch (hex) {
        case 'a':
        case 'A': return 10;
        case 'b':
        case 'B': return 11;
        case 'c':
        case 'C': return 12;
        case 'd':
        case 'D': return 13;
        case 'e':
        case 'E': return 14;
        case 'f':
        case 'F': return 15;
        default: return -1;
    }
}

// 2. Printing the Bit Pattern String
static void pvt_print_hex_bits(unsigned char hex) {
    int val = pvt_hex_to_dec(hex);
    if (val == -1) return;

    // Extract bits using bitwise masking
    for (int i = 3; i >= 0; i--) {
        printf("%d", (val >> i) & 1);
    }
    printf("\n");
}

static void pvt_print_8_bits(uint8_t bits) {
    printf("0b");
    for (int i = 7; i >= 0; i--) {
        printf("%d", (bits >> i) & 1);
    }
}

constexpr uint8_t continue_mask = 0b00111111;  // 0x3F
constexpr uint8_t continue_bits = 0b10000000;  // 0x80

// what do we do here? Take 1 to 4 bytes of UTF-8 and re-constitute as Unicode codepoint?
// we probably need different methods to decode utf-8 to Unicode 32, or Unicode 16 (surrogate pairs?)
// when we save a JSON text to a JSON file, we'll be writing UTF-8, so for this we probably don't need
// to generate surrogates or UTF-16/32.

// But we do need to be able to validate the UTF-8 in the JSON text, so we don't necessarily need to
// decode, just verify/validate.

// assumes the UTF-8 bytes are correct and well-ordered. No error checking is done here.
// Returns the decoded Unicode codepoint value or UINT32_MAX if error occurred.
static uint32_t pvt_decode_utf8(const uint32_t num_bytes, uint8_t const utf8_bytes[]) {
    uint32_t codepoint = 0;
    switch  (num_bytes) {
        case 1:
            codepoint = utf8_bytes[0];
            break;
        case 2:
            codepoint = codepoint | ( ~0b11000000   & utf8_bytes[0]) << 6;
            codepoint = codepoint | ( continue_mask & utf8_bytes[1]);
            break;
        case 3:
            codepoint = codepoint | ( ~0b11100000   & utf8_bytes[0]) << 12;
            codepoint = codepoint | ( continue_mask & utf8_bytes[1]) << 6;
            codepoint = codepoint | ( continue_mask & utf8_bytes[2]);
            break;
        case 4:
            codepoint = codepoint | ( ~0b11110000   & utf8_bytes[0]) << 18;
            codepoint = codepoint | ( continue_mask & utf8_bytes[1]) << 12;
            codepoint = codepoint | ( continue_mask & utf8_bytes[2]) << 6;
            codepoint = codepoint | ( continue_mask & utf8_bytes[3]);
            break;
        default:
            // here we could iterate dynamically for possible encodings past 4 bytes.
            // for now, we take no action and will return UINT32_MAX
            codepoint = UINT32_MAX;
            break;
    }

    return codepoint;
}



// GEMINI AI READ: This is my work in progress. Do not delete it. Do not modify. Ignore this function.
static StringBuilder * pvt_encode_utf8( JsonContext *context, JsonParseError *error, const uint32_t codepoint, StringBuilder *sb) {
    /**
     *  Rules for encoding Unicode codepoint into UTF-8:
     *  0. Unicode points U+D800 - U+DFFF are reserved as surrogate pairs in UTF-16 and are not allowed. Error.
     *  1. If codepoint <= 127, enccode as single byte of same value:
     *  2. If codepoint <= 0x07FF, encode as two UTF-8 bytes
     *
     *  The 1024 points in the range U+D800–U+DBFF are known as high-surrogate code points,
     *  and code points in the range U+DC00–U+DFFF (1024 code points) are known as low-surrogate code points.
     */


    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
        // error, reserved for surrogate pairs
        if (codepoint < 0xDC00 ) {
            // high/leading surrogate
            snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
        "'U+%.4X' is reserved as a high/leading surrogate and cannot be used as a codepoint", codepoint);
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_RESERVED_FOR_HIGH_SURROGATE, error->message);
            return nullptr;
        } else {
            // low/ trailing surrogate
            snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
        "'U+%.4X' is reserved as a low/trailing surrogate and cannot be used as a codepoint", codepoint);
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_RESERVED_FOR_LOW_SURROGATE, error->message);
            return nullptr;
        }
    }

    if (codepoint <= 127 ) {
        sb_append_char(sb, (char)codepoint);
    } else if (codepoint <= 0x07FF ) {
        // encode as two UTF-8 bytes
        //110xxxxx 10xxxxxx
        uint8_t first  = 0b11000000    | (uint8_t)( codepoint >> 6 );
        uint8_t second = continue_bits | (uint8_t)( codepoint & continue_mask );
        sb_append_char(sb, (char)first);
        sb_append_char(sb, (char)second);

    } else if (codepoint <= 0xFFFF) {
        // encode as three UTF-8 bytes
        // 1110xxxx 10xxxxxx 10xxxxxx
        uint8_t first  = 0b11100000    | (uint8_t)( codepoint >> 12 ) ;
        uint8_t second = continue_bits | (uint8_t)( codepoint >> 6 & continue_mask);
        uint8_t third  = continue_bits | (uint8_t)( codepoint      & continue_mask );
        sb_append_char(sb, (char)first);
        sb_append_char(sb, (char)second);
        sb_append_char(sb, (char)third);

    } else if (codepoint <= 0x10FFFF) {
        // encode as four UTF-8 bytes
        // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        uint8_t first   = 0b11110000    | (uint8_t)( codepoint >> 18 ) ;
        uint8_t second  = continue_bits | (uint8_t)( codepoint >> 12 & continue_mask);
        uint8_t third   = continue_bits | (uint8_t)( codepoint >>  6 & continue_mask);
        uint8_t fourth  = continue_bits | (uint8_t)( codepoint       & continue_mask );
        sb_append_char(sb, (char)first);
        sb_append_char(sb, (char)second);
        sb_append_char(sb, (char)third);
        sb_append_char(sb, (char)fourth);

    } else {
        // error, codepoint out of range
        snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
    "'U+%.4X' is out of range and cannot be used as a codepoint", codepoint);
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_CODEPOINT_OUT_OF_RANGE, error->message);
        return nullptr;
    }

    return sb;
}

static uint32_t pvt_parse_hex_impl(JsonContext *context, JsonParseError *error, const uint32_t num_chars) {
    uint32_t result = 0;
    for (uint32_t i = 0; i < num_chars; i++) {
        const char next_char = pvt_peek_char(context);
        if (next_char == '\0') {
            context->parse_end = context->current_index;
            snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
                "unexpected EOF while parsing hex digit. (expected %d hex digits, got %d)",  num_chars, i);
            pvt_record_error(context, error, JSON_ERR_UNEXPECTED_EOF, error->message);
            //  clang/clion linter doesn't see that record_error changes err_type.
            //   without this, it erroneously reports of unreachable code in calling methods
            error->err_type = JSON_ERR_UNEXPECTED_EOF;
            return 0;
        }
        if (!isxdigit((unsigned char)next_char)) {
            context->parse_end = context->current_index;
            snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
                "invalid hex digit in Unicode escape: '%c'. (expected %d hex digits, got %d)", next_char, num_chars, i);
            pvt_record_error(context, error, JSON_ERR_INVALID_UNICODE_ESCAPE, error->message);
            return 0;
        }

        // convert hex digit to uint and shift into result.
        const uint32_t dec_value = (uint32_t)pvt_hex_to_dec(next_char);  // dec_value: 0 - 15
        result = result * 16 | dec_value;
        pvt_advance(context, 1);
    }
    return result;
}

// try to parse 6 hex bytes from the stream. Report in error if we didn't find 6 hex bytes.
static uint32_t pvt_parse_hex6(JsonContext *context, JsonParseError *error) {
    return pvt_parse_hex_impl(context, error, 6);
}

// try to parse 4 hex bytes from the stream. Report in error if we didn't find 4 hex bytes.
static uint16_t pvt_parse_hex4(JsonContext *context, JsonParseError *error) {
    return pvt_parse_hex_impl(context, error, 4);
}

//  expect a valid continuation byte from 0x80 - 0xBF at current index
// Return true if found, otherwise return false and report error
static bool pvt_validate_utf8_continuation_byte(
    JsonContext *context, JsonParseError *error,
    uint8_t current_byte, uint8_t start_range, uint8_t end_range) {

    uint8_t next_byte = pvt_peek_char(context);

    if ( !( next_byte >= start_range && next_byte <= end_range )) {
        snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
    "'0x%.2X' is an invalid UTF-8 continuation byte after '0x%.2X'", next_byte, current_byte);
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_INVALID_UTF8_CONTINUATION_BYTE, error->message);
        return false;
    }

    return true;
}

static bool pvt_validate_utf8(JsonContext *context, JsonParseError *error,  StringBuilder *sb) {
    uint8_t stream_bytes[4] = {};
    uint8_t lead_byte = pvt_peek_char(context);;
    uint32_t num_bytes = 0;
    stream_bytes[num_bytes++] = lead_byte;

    // -----------------------------------------------------------------
    //          ONE BYTE (ASCII)
    // -----------------------------------------------------------------

    if (lead_byte <= 127 ) {
        sb_append_char(sb, (char)lead_byte);
        pvt_advance(context, 1);
        return true;
    }

    // -----------------------------------------------------------------
    //          TWO BYTES
    // -----------------------------------------------------------------

    uint8_t current_byte = lead_byte;


    // if first byte is C2-DF, possible start of 2-byte sequence.
    if (lead_byte >= 0xC2 && lead_byte <= 0xDF ) {
        pvt_advance(context, 1);
        // expect 0x80-BF to follow.
        if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
            return false;
        }
        current_byte = (uint8_t)pvt_peek_char(context);
        stream_bytes[num_bytes++] = current_byte;
        pvt_advance(context, 1);
        //happy path
        sb_append_char(sb, (char)stream_bytes[0]);
        sb_append_char(sb, (char)stream_bytes[1]);
        return true;
    }

    // -----------------------------------------------------------------
    //      THREE BYTES
    // -----------------------------------------------------------------

    // if the first byte is E0-EF, expect 2 following bytes 80-BF with some exceptions.
    //      EDA0+ is the start of a surrogate pair and not allowed
    if ( lead_byte >= 0xE0 && lead_byte <= 0xEF ) {
        if (lead_byte == 0xE0) {
            pvt_advance(context, 1);
            //  expect next byte in 0xA0-BF, then 0x80-BF
            //    If the second byte is 0x80 to 0x9F, overlong sequence
            current_byte = (uint8_t)pvt_peek_char(context);
            if ( current_byte >= 0x80 && current_byte <= 0x9F) {
                snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
            "'0x%.2X' is an invalid UTF-8 continuation byte and an overlong sequence", current_byte);
                context->parse_end = context->current_index;
                pvt_record_error(context, error, JSON_ERR_OVERLONG_SEQUENCE, error->message);
                return false;
            }
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0xA0, 0xBF )) {
                return false;
            }
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_peek_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            //happy path
            sb_append_char(sb, (char)stream_bytes[0]);
            sb_append_char(sb, (char)stream_bytes[1]);
            sb_append_char(sb, (char)stream_bytes[2]);
            return true;
        }
        if ( lead_byte >= 0xE1 && lead_byte <= 0xEC ) {
            pvt_advance(context, 1);
            // expect next 2 bytes in 0x80-BF
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_peek_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_peek_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            //happy path
            sb_append_char(sb, (char)stream_bytes[0]);
            sb_append_char(sb, (char)stream_bytes[1]);
            sb_append_char(sb, (char)stream_bytes[2]);
            return true;
        }
        if (lead_byte == 0xED) {
            pvt_advance(context, 1);
            //expect next byte in 0x80-9F, then x80-BF
            // if second byte in 0xA0 to 0xBF, encoding a surrogate : forbidden
            current_byte = (uint8_t)pvt_peek_char(context);
            if ( current_byte >= 0xA0 && current_byte <= 0xAF) {
                snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
            "'0x%.2X' is an invalid UTF-8 continuation byte and reserved for high surrogates", current_byte);
                context->parse_end = context->current_index;
                pvt_record_error(context, error, JSON_ERR_RESERVED_FOR_HIGH_SURROGATE, error->message);
                return false;
            }
            if ( current_byte >= 0xB0 && current_byte <= 0xBF) {
                snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
            "'0x%.2X' is an invalid UTF-8 continuation byte and reserved for low surrogates", current_byte);
                context->parse_end = context->current_index;
                pvt_record_error(context, error, JSON_ERR_RESERVED_FOR_LOW_SURROGATE, error->message);
                return false;
            }
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0x9F )) {
                return false;
            }
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_peek_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            //happy path
            sb_append_char(sb, (char)stream_bytes[0]);
            sb_append_char(sb, (char)stream_bytes[1]);
            sb_append_char(sb, (char)stream_bytes[2]);
            return true;
        }
        if ( lead_byte == 0xEE || lead_byte == 0xEF) {
            pvt_advance(context, 1);
            // expect next 2 bytes in 0x80-BF
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_peek_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_peek_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            //happy path
            sb_append_char(sb, (char)stream_bytes[0]);
            sb_append_char(sb, (char)stream_bytes[1]);
            sb_append_char(sb, (char)stream_bytes[2]);
            return true;
        }
    }


    // -----------------------------------------------------------------
    //          FOUR BYTES
    // -----------------------------------------------------------------

    // if first byte is F0-F4, expect 3 following bytes 80-BF with some exceptions.
    if ( lead_byte >= 0xF0 && lead_byte <= 0xF4 ) {
        if (lead_byte == 0xF0) {
            pvt_advance(context, 1);
            // expect next byte in 0x90-BF, then next two in x80-BF
            //  If the second byte is 0x80 to 0x8F, overlong sequence.
            current_byte = (uint8_t)pvt_peek_char(context);
            if ( current_byte >= 0x80 && current_byte <= 0x8F) {
                snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
            "'0x%.2X' is an invalid UTF-8 continuation byte and an overlong sequence", current_byte);
                context->parse_end = context->current_index;
                pvt_record_error(context, error, JSON_ERR_OVERLONG_SEQUENCE, error->message);
                return false;
            }
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x90, 0xBF )) {
                return false;
            }
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_peek_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_peek_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            //happy path
            sb_append_char(sb, (char)stream_bytes[0]);
            sb_append_char(sb, (char)stream_bytes[1]);
            sb_append_char(sb, (char)stream_bytes[2]);
            sb_append_char(sb, (char)stream_bytes[3]);

            return true;
        }

        if ( lead_byte >= 0xF1 && lead_byte <= 0xF3 ) {
            pvt_advance(context, 1);
            //expect next 3 bytes in x80-BF
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_peek_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_peek_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_peek_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            //happy path
            sb_append_char(sb, (char)stream_bytes[0]);
            sb_append_char(sb, (char)stream_bytes[1]);
            sb_append_char(sb, (char)stream_bytes[2]);
            sb_append_char(sb, (char)stream_bytes[3]);

            return true;
        }

        if (lead_byte == 0xF4) {
            pvt_advance(context, 1);
            // expect next byte in 0x80-8F, then next two in x80-BF
            //  if second byte > 0x90, this exceeds legal Unicode limit
            current_byte = (uint8_t)pvt_peek_char(context);
            if ( current_byte >= 0x90 ) {
                snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
            "'0x%.2X' is an invalid UTF-8 continuation byte and is out of range", current_byte);
                context->parse_end = context->current_index;
                pvt_record_error(context, error, JSON_ERR_CODEPOINT_OUT_OF_RANGE, error->message);
                return false;
            }

            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0x8F )) {
                return false;
            }
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_peek_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_peek_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            //happy path
            sb_append_char(sb, (char)stream_bytes[0]);
            sb_append_char(sb, (char)stream_bytes[1]);
            sb_append_char(sb, (char)stream_bytes[2]);
            sb_append_char(sb, (char)stream_bytes[3]);

            return true;
        }
    }

    // C0-C1 is invalid.
    if (lead_byte == 0xC0 || lead_byte == 0xC1 ) {
        snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
    "'0x%.2X' is an invalid UTF-8 start byte and an overlong sequence", lead_byte);
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_OVERLONG_SEQUENCE, error->message);
        return false;
    }

    snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
"'0x%.2X' is an invalid UTF-8 start byte", lead_byte);
    context->parse_end = context->current_index;
    pvt_record_error(context, error, JSON_ERR_INVALID_UTF8_START_BYTE, error->message);
    return false;
}

// assumes pvt_peek_char(context) == 'u' or 'U' and the previous character was a backslash '\'
static StringBuilder * pvt_parse_unicode_escape( JsonContext *context, JsonParseError *error, Arena *arena, StringBuilder *sb_out ) {
    if (pvt_peek_char(context) == 'U') {
        if ( !jsonp_is_context_config_flag_set(context, JSON_CONFIG_ALLOW_UNICODE_U_ESCAPE)) {
            // got a \U (uppercase U) Unicode escape but flag is not enabled
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_INVALID_ESCAPE_SEQUENCE,
                "Invalid Unicode escape sequence '\\U'. Set config flag JSON_CONFIG_ALLOW_UNICODE_U_ESCAPE "
                "to enable this extension");
            return nullptr;
        }
        pvt_advance(context, 1);  // // consume 'U'
        // roblib enhancement. Allows a Unicode codepoint without surrogates.
        uint32_t full_codepoint = pvt_parse_hex6(context, error);
        if (error->err_type != JSON_ERR_NONE) {
            return nullptr;  // we got an error in parse_hex6()
        }
        // full_codepoint contains the Unicode codepoint to encode as UTF-8
        // encode to UTF-8 and write to sb.
        pvt_encode_utf8(context, error, full_codepoint, sb_out);
        return sb_out;
    }

    pvt_advance(context, 1);  // consume 'u'
    uint16_t cp1 = pvt_parse_hex4(context, error);

    if (error->err_type != JSON_ERR_NONE) {
        return nullptr;  // we got an error in parse_hex4()
    }
    if ( cp1 >= 0xDC00 && cp1 <= 0xDFFF ) {
        // A low surrogate that wasn't preceded by a high surrogate. This is an error.
        snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
            "expected high surrogate \\uD800-\\uDBFF to precede low surrogate '\\u%4X', but none found", cp1);
        // todo (rob) update parse state to report error position as before this surrogate
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_NO_PRECEDING_HIGH_SURROGATE, error->message);
        return nullptr;
    }
    uint32_t cp = cp1;

    if (cp1 >= 0xD800 && cp1 <= 0xDBFF ) {
        // High surrogate, expect the low surrogate to follow.
        // RFC 8259: To escape an extended character (>U+FFFF), it must be
        // represented as a 12-character sequence encoding the UTF-16 surrogate pair.
        // Non-standard escapes like \UXXXXXXXX are not supported.
        const char current_char = pvt_peek_char(context);

        // Check for enough remaining characters safely
        if (current_char != '\\' || pvt_peek_next_char(context) != 'u') {
            // no following low surrogate.
            snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
                "expected low surrogate escape \\uDC00-\\uDFFF to follow high surrogate '\\u%4X'", cp1);
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_NO_FOLLOWING_LOW_SURROGATE, error->message);
            return nullptr;
        }
        //we have a second Unicode escape immediately after the first.
        pvt_advance(context, 2);  // consume '\u'
        //Possibly high- / low-surrogate pair
        uint16_t cp2 = pvt_parse_hex4(context, error);
        if (error->err_type != JSON_ERR_NONE) {
            return nullptr;  // we got an error in parse_hex4()
        }

        if ( !(cp2 >= 0xDC00 && cp2 <= 0xDFFF )) {
            // no following low surrogate.
            snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
                "expected low surrogate to follow '\\u%4X', but found '\\u%4X' instead", cp1, cp2);
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_NO_FOLLOWING_LOW_SURROGATE, error->message);
            return nullptr;
        }


        // combine : 0x10000 + ((cp1 - 0xD800) << 10) + (cp2 - 0xDC00).
        cp = 0x10000 + ((cp1 - 0xD800) << 10) + (cp2 - 0xDC00);
    }

    // cp contains the Unicode codepoint to encode as UTF-8
    // encode to UTF-8 and write to sb.
    // write_utf8(context, error, cp, sb_out);
    pvt_encode_utf8(context, error, cp, sb_out);
    return sb_out;
}



constexpr char QUOTE           = 0x22;  // "
constexpr char REVERSE_SOLIDUS = 0x5c;  // \  backslash

static JsonValue * pvt_parse_string(JsonContext *context, JsonParseError *error, Arena *arena ) {
    StringBuilder sb;
    sb_init(&sb, 16, "");

    // we examine chars in the stream until we find:
    // 1. a closing quote, which is a quote not preceded by the backlash (reverse solidus)
    // 2. EOF, AKA null terminator, \x00
    JsonValue *value = nullptr;
    pvt_advance(context, 1); // Skip the opening quote
    unsigned char current_byte = (unsigned char )pvt_peek_char(context);
    while (current_byte) {
        if (current_byte == QUOTE) {
            // happy case. We found the terminating quote
            value = arena_alloc(arena, sizeof(JsonValue) );
            value->type = JSON_STRING;

            // Ensure the result is null-terminated from the StringBuilder
            // before copying into the arena.
            size_t len = sb.length;
            char * str_value = arena_alloc(arena, len + 1);
            memcpy(str_value, sb.buffer, len);
            str_value[len] = '\0';

            value->u.string = str_value;

            pvt_advance(context, 1); // consume the terminating quote
            sb_destroy(&sb);
            return value;
        }

        if (current_byte == REVERSE_SOLIDUS) {
            pvt_advance(context, 1); // Skip the backslash
            current_byte = (unsigned char )pvt_peek_char(context);

            if (current_byte == '\0') {
                pvt_record_error(context, error, JSON_ERR_UNEXPECTED_EOF, "Unexpected EOF after backslash");
                sb_destroy(&sb);
                return nullptr; // Unexpected EOF
            }

            // Validate escape sequence
            switch (current_byte) {
                case '"':
                case '\\':
                case '/':
                    sb_append_char(&sb, (char)current_byte);
                    pvt_advance(context, 1);
                    break;
                case 'b':
                    sb_append_char(&sb, '\b');
                    pvt_advance(context, 1);
                    break;
                case 'f':
                    sb_append_char(&sb, '\f');
                    pvt_advance(context, 1);
                    break;
                case 'n':
                    sb_append_char(&sb, '\n');
                    pvt_advance(context, 1);
                    break;
                case 'r':
                    sb_append_char(&sb, '\r');
                    pvt_advance(context, 1);
                    break;
                case 't':
                    sb_append_char(&sb, '\t');
                    pvt_advance(context, 1);
                    break;
                case 'u':
                case 'U':
                    // RFC 8259: \u followed by 4 hex digits
                    // roblib addition \U followed by 6 hex digits is a codepoint,
                    //  no surrogates required!
                    StringBuilder *result = pvt_parse_unicode_escape(context, error, arena, &sb);
                    if (!result) {
                        context->parse_end = context->current_index;
                        sb_destroy(&sb);
                        return nullptr;
                    }
                    break;
                default:
                    context->parse_end = context->current_index;
                    snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "invalid escape sequence: '\\%c'", current_byte);
                    pvt_record_error(context, error, JSON_ERR_INVALID_ESCAPE_SEQUENCE, error->message);
                    sb_destroy(&sb);
                    return nullptr;
            }

        } else if ( current_byte <= 0x1F) {
            // RFC 8259: Control characters U+0000 through U+001F MUST be escaped.
            // This means the literal bytes cannot appear here.
            context->parse_end = context->current_index;
            snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "unescaped control character: 0x%.2X", current_byte);
            pvt_record_error(context, error, JSON_ERR_UNESCAPED_CONTROL_CHAR, error->message);
            sb_destroy(&sb);
            return nullptr;
        } else {
            // Here we validate a string of UTF-8 characters
            if (! pvt_validate_utf8(context, error, &sb)) return nullptr;
        }
        current_byte = (unsigned char)pvt_peek_char(context);
    }

    context->parse_end = context->current_index;
    pvt_record_error(context, error, JSON_ERR_UNTERMINATED_STRING, "missing closing quote '\"' ");
    sb_destroy(&sb);
    return nullptr;
}

typedef struct CharBuffer {
    size_t length;
    uint8_t buffer[1024];
} CharBuffer;

static void pvt_add_char_to_buffer( CharBuffer * cb, char c) {
    if (cb->length > sizeof (cb->buffer) - 1) return;

    cb->buffer[cb->length++] = (uint8_t)c;
}

static JsonValue * pvt_parse_number(JsonContext *context, JsonParseError *error, Arena *arena ) {
    JsonValue *value = nullptr;
    CharBuffer cb = {}; // todo this is a fixed buffer of 1024 bytes. Max length of number we can parse.


    //context->parse_start has the first character in this number string, which we'll need to convert the parsed number
    char cur_char = pvt_peek_char(context);
    if (cur_char == '.') {
        snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
    "expected digit '0'-'9' before the decimal point");
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_INVALID_NUMBER_FORMAT, error->message);
        return nullptr;
    }

    if (cur_char == '-') {
        pvt_add_char_to_buffer(&cb, cur_char);
        pvt_advance(context, 1);  // optional '-' is fine.
        //must be followed by a digit
        char c = pvt_peek_char(context);
        if ( !(c >= '0' && c <= '9')) {
            int written =  snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
        "expected digit '0'-'9' after '-', got ");
            if (written > 0) {
                pvt_format_error_message_char(ERROR_MSG_BUFFER_SIZE - written,
                    error->message + written, pvt_peek_char(context) );
            }
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_INVALID_NUMBER_FORMAT, error->message);
            return nullptr;
        }
    }

    // Number *must* start with 0 exactly once, or a digit 1-9 exactly once, followed by zero or more digits 0-9
    cur_char = pvt_peek_char(context);
    if ( !( cur_char >= '0' && cur_char <= '9')) {
        int written =  snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
            "numbers must start with a digit '0'-'9', got ");
        if (written > 0) {
            pvt_format_error_message_char(ERROR_MSG_BUFFER_SIZE - written,
                error->message + written, pvt_peek_char(context) );
        }
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_INVALID_NUMBER_FORMAT, error->message);
        return nullptr;
    }

    // -----------------------------------------------------------------
    //      INTEGER PART
    // -----------------------------------------------------------------

    // here we know current char is '0'-'9'
    if (cur_char == '0') {
        pvt_add_char_to_buffer(&cb, cur_char);
        pvt_advance(context, 1);
        char c = pvt_peek_char(context);
        // leading zero only valid if next character is a period, e or E
        if ( c >= '0' && c <= '9') {
            int written =  snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
                "invalid leading zero. expected '.', 'e', or 'E', got ");
            if (written > 0) {
                pvt_format_error_message_char(ERROR_MSG_BUFFER_SIZE - written,
                    error->message + written, pvt_peek_char(context) );
            }
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_INVALID_NUMBER_FORMAT, error->message);
            return nullptr;
        }
        // leading zero only valid if next character is a period, e or E and not a digit
        if (  c != '.' && c != 'e' && c != 'E' ) {
            // we parsed a zero
            value = arena_alloc(arena, sizeof(JsonValue) );
            value->type = JSON_LONG;
            value->u.n_long = 0;
            return value;
        }
    } else {
        // cur_char '1'-'9'
        while ( pvt_peek_char(context) >= '0' && pvt_peek_char(context) <= '9' ) {
            pvt_add_char_to_buffer(&cb, pvt_peek_char(context));
            pvt_advance(context, 1);
        }
    }

    // -----------------------------------------------------------------
    //          PARSE AS INTEGER
    // -----------------------------------------------------------------

    cur_char = pvt_peek_char(context);
    if ( cur_char != '.' && cur_char != 'e' && cur_char != 'E') {
        // we parsed an integer
        value = arena_alloc(arena, sizeof(JsonValue) );
        value->type = JSON_LONG;
        errno = 0; // Reset errno before the calls
        char *str_end =  nullptr;
        char const * const num_start_pointer = context->json_text +  context->parse_start;
        // long val = strtol(num_start_pointer, &str_end, 10);
        // printf("cb.buffer=%s\n", (char const *)cb.buffer);

        long val = strtol( (char const *)cb.buffer, &str_end, 10);


        if (errno == ERANGE) {
            // Promotion: If too big for long, use double to preserve magnitude (even if it becomes Infinity)
            // todo (rob) warn? exit with error depending on config flag?
            // what other errors are possible?
            printf("errno=%d, cb.buffer=%s\n", errno, (char const *)cb.buffer);
            value->type = JSON_DOUBLE;
            value->u.n_double = strtod(num_start_pointer, &str_end);
        } else {
            value->u.n_long = val;
        }
        return value;
    }


    // -----------------------------------------------------------------
    //      FRACTIONAL PART
    // -----------------------------------------------------------------

    if (cur_char == '.' )  {
        pvt_add_char_to_buffer(&cb, context->decimal_separator);  // use the locale-defined separator
        pvt_advance(context, 1);  // consume optional '.'
        // expect one number
        char c = pvt_peek_char(context);
        if ( !(c >= '0' && c <= '9') ) {
            int written =  snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "expected digit '0'-'9' after the decimal point, got ");
            if (written > 0) {
                pvt_format_error_message_char(ERROR_MSG_BUFFER_SIZE - written,
                    error->message + written, pvt_peek_char(context) );
            }
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_INVALID_NUMBER_FORMAT, error->message);
            return nullptr;
        }
        pvt_add_char_to_buffer(&cb, c);
        pvt_advance(context, 1); // consume first digit

        // consume next N digits
        while ( pvt_peek_char(context) >= '0' && pvt_peek_char(context) <= '9' ) {
            pvt_add_char_to_buffer(&cb, pvt_peek_char(context));
            pvt_advance(context, 1);
        }
    }

    cur_char = pvt_peek_char(context);
    if ( cur_char == 'e' || cur_char == 'E' ) {
        char const e_char = cur_char;  // save actual letter case for reporting
        pvt_add_char_to_buffer(&cb, cur_char);
        pvt_advance(context, 1);
        char c = pvt_peek_char(context);
        if ( c == '-' || c == '+' ) {
            pvt_add_char_to_buffer(&cb, c);
            pvt_advance(context, 1); // consume optional -/+ char
        }
        // expect one number
        c = pvt_peek_char(context);
        if ( !(c >= '0' && c <= '9')) {
            int written =  snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "expected digit '0'-'9' after '%c', got ", e_char);
            if (written > 0) {
                pvt_format_error_message_char(ERROR_MSG_BUFFER_SIZE - written,
                    error->message + written, pvt_peek_char(context) );
            }
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_INVALID_NUMBER_FORMAT, error->message);
            return nullptr;
        }
        pvt_add_char_to_buffer(&cb, c);
        pvt_advance(context, 1); // consume first digit
        // consume next N digits
        while ( pvt_peek_char(context) >= '0' && pvt_peek_char(context) <= '9') {
            pvt_add_char_to_buffer(&cb, pvt_peek_char(context) );
            pvt_advance(context, 1);
        }
    }

    // -----------------------------------------------------------------
    //          PARSE AS FLOATING POINT
    // -----------------------------------------------------------------

    // we've now parsed a float number
    value = arena_alloc(arena, sizeof(JsonValue) );
    value->type = JSON_DOUBLE;
    errno = 0; // Reset errno before the calls
    char *str_end =  nullptr;
    char const * const num_start_pointer = context->json_text +  context->parse_start;
    // todo (rob) if context->decimal_separator is not '.' we have to substitute the '.' in the JSON text
    // printf("cb.buffer=%s\n", (char const *)cb.buffer);

    // value->u.n_double = strtod(num_start_pointer, &str_end)
    value->u.n_double = strtod((char const *)cb.buffer, &str_end);

    // Note: If strtod overflows, u.n_double will be +/- Infinity (HUGE_VAL).

    if (errno || str_end != context->current_ptr) {
        // printf("Error calling strtod_l: errno:%d, *str_end:%p, cur_ptr:%p\n", errno, str_end, context->current_ptr);
    }
    if (errno || str_end != context->current_ptr) {
        // printf("Error calling strtod_l: errno:%d, *str_end:%p, cur_ptr:%p\n", errno, str_end, context->current_ptr);
    }
    return value;
}

// RSL: "Regex Start of Line"
#define RSL      "^"
// not used, but this defines what a JSON number is, so it's convenient to have around.
static const char * const REGEX_NUMBER_STR = RSL "(-?(0|([1-9][0-9]*))(\\.[0-9]+)?([eE][-+]?[0-9]+)?)";

static JsonValue *  pvt_parse_literal_impl(  JsonContext *context,
                                                JsonParseError *error,
                                                JsonValue *literal,
                                                char const *key_word ) {

    context->parse_start = context->current_index;
    uint32_t current_index = context->current_index;
    char const * keyword_ptr = key_word;
    char cur_char = pvt_peek_char(context);
    enum json_error_type_e err_type = JSON_ERR_NONE;

    while ( *keyword_ptr != '\0' && cur_char != '\0') {
        if (*keyword_ptr != cur_char) {
            err_type = JSON_ERR_UNEXPECTED_TEXT;
            snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
                "unexpected character: '%c', expected '%s'", cur_char, key_word);
            break;
        }
        pvt_advance(context, 1);
        cur_char = pvt_peek_char(context);
        keyword_ptr++;
    }



    context->parse_end = current_index;

    if ( err_type == JSON_ERR_NONE && *keyword_ptr != '\0') {
        // unexpected end of text
        err_type = JSON_ERR_UNEXPECTED_EOF;
        snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "unexpected EOF, expected '%s'", key_word);
    }
    if (err_type != JSON_ERR_NONE) {
        pvt_record_error(context, error, err_type, error->message);
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

static JsonValue *  pvt_parse_true(JsonContext *context, JsonParseError *error ) {
    return pvt_parse_literal_impl(context, error, &JSON_TRUE_VALUE, JSON_KEYWORD_TRUE );
}

static JsonValue *  pvt_parse_false(JsonContext *context, JsonParseError *error ) {
    return pvt_parse_literal_impl(context, error, &JSON_FALSE_VALUE, JSON_KEYWORD_FALSE );
}

static JsonValue *  pvt_parse_null(JsonContext *context, JsonParseError *error ) {
    return pvt_parse_literal_impl(context, error, &JSON_NULL_VALUE, JSON_KEYWORD_NULL );
}

static JsonValue *pvt_parse_value(JsonContext *context, JsonParseError *error, Arena *arena ) {
    pvt_skip_whitespace(context);
    context->parse_start = context->current_index;
    JsonValue *value = nullptr;

    /**
    *while ((n = input->read(ctx, buf, sizeof(buf))) > 0)
    parser_feed(parser, buf, n);
     */

    switch (pvt_peek_char(context)) {
        case '{': /* Handle object */
            value = pvt_parse_object(context, error, arena);
            break;
        case '[': /* Handle array */
            value = pvt_parse_array(context, error, arena);
            break;
        case '"': /* Handle string */
            value = pvt_parse_string(context, error, arena);
            break;
        case '-': case '.': case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7': case '8': case '9':
            /* Handle number */
            value = pvt_parse_number(context,error, arena);
            break;
        case 't': /* Handle true */
            value = pvt_parse_true(context, error);
            break;
        case 'f': /* Handle false */
            value = pvt_parse_false(context, error);
            break;
        case 'n': /* Handle null */
            value = pvt_parse_null(context, error) ;
            break;

        default:
            if (error) {
                int written = snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "unexpected character: ");
                if (written > 0) {
                    pvt_format_error_message_char(ERROR_MSG_BUFFER_SIZE - written,
                        error->message + written, pvt_peek_char(context) );
                }

                context->parse_end = context->current_index;
                pvt_record_error(context, error, JSON_ERR_UNEXPECTED_TEXT, error->message);
                return nullptr;
            }
            break;

    }

    return value;
}

static JsonValue * pvt_jsonp_parse_impl(JsonContext *context, const char *json_text, JsonParseError *error, Arena *arena) {
    if (!json_text) {
        *error = (JsonParseError){ .json=json_text, .message = "null json text", .err_type = JSON_ERR_NULL_TEXT};
        return nullptr;
    }
    if (json_text[0] == '\0') {
        *error = (JsonParseError){.json=json_text, .message = "empty json text", .err_type = JSON_ERR_EMPTY_TEXT};
        return nullptr;
    }

    if (pvt_is_rejected_due_to_bom(context, json_text, error)) return nullptr;

    pvt_skip_whitespace(context);
    if ( pvt_peek_char(context) == '\0') {
        *error = (JsonParseError){.json=json_text, .message = "empty json text", .err_type = JSON_ERR_EMPTY_TEXT,
        .first_bad_char = context->current_index, .parse_end = context->current_index};
        return nullptr;
    }


    //A JSON value MUST be an object, array, number, or string, or one of the following three literal names:
    //  false, null, true

    error->err_type = JSON_ERR_NONE;
    JsonValue *value =  pvt_parse_value(context, error, arena);
    if ( error->err_type != JSON_ERR_NONE) {
        return nullptr;
    }

    pvt_skip_whitespace(context);

    if (pvt_peek_char(context) != '\0') {
        //we parsed the root value, but there is still text remaining in the JSON text, which is an error
        pvt_record_error(context, error, JSON_ERR_UNEXPECTED_TEXT, "unexpected extra text after parsing a valid JSON value");
        return nullptr;
    }
    return value;
}



// -----------------------------------------------------------------
//          BOM CHECKING
// -----------------------------------------------------------------

// assumes json_test is not null
static bool pvt_starts_with_bom(const char json_text[static 1], const uint32_t n_bytes, char const bom_bytes[static n_bytes]) {
    for (uint32_t i = 0; i < n_bytes; ++i) {
        if (json_text[i] != bom_bytes[i]) {
            return false;
        }
    }
    return true;
}

static bool pvt_is_rejected_due_to_bom(JsonContext *context, const char *json_text, JsonParseError *error) {
    // UTF-16 and UTF-32 BOMs always cause failure
    if (pvt_starts_with_bom(json_text, 2, BOM_UTF16_BE) ||
            pvt_starts_with_bom(json_text, 2, BOM_UTF16_LE)) {
                pvt_advance(context, 2);
                context->parse_end = context->current_index;
                pvt_record_error(context, error, JSON_ERR_UNEXPECTED_UTF16_ENCODING, "This document appears to be encoded with UTF-16. Expected UTF-8.");
                return true;
            }
    if (pvt_starts_with_bom(json_text, 4, BOM_UTF32_BE) ||
            pvt_starts_with_bom(json_text, 4, BOM_UTF32_LE)) {
                pvt_advance(context, 4);
                context->parse_end = context->current_index;
                pvt_record_error(context, error, JSON_ERR_UNEXPECTED_UTF32_ENCODING, "This document appears to be encoded with UTF-32. Expected UTF-8.");
                return true;
            }


    // utf8 BOM behavior controlled by flag JSON_CONFIG_FAIL_ON_INITIAL_BOM
    if (pvt_starts_with_bom(json_text, 3, BOM_UTF8)  ) {
        if (jsonp_is_context_config_flag_set(context, JSON_CONFIG_FAIL_ON_INITIAL_BOM)) {
            pvt_advance(context, 3);
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_BOM_NOT_ALLOWED,
                "This document begins with a byte-order-mark (BOM), which is not allowed "
                "when config flag 'JSON_CONFIG_FAIL_ON_INITIAL_BOM' has been set.");
            return true;
        }
        // todo (rob) do we count these 3 bytes as part of the document for error reporting, or do we just skip over
        // them with json_text = json_text + 3;?
        pvt_advance(context, 3);
    }
    return false;
}



// -----------------------------------------------------------------
//      Decimal Seperator Char
// -----------------------------------------------------------------

// Assumes a simple ASCII character. If the local's dec separator is multibyte or non-printable, returns the default
// '.'
char pvt_get_locale_decimal_separator_char() {
    struct lconv *lc = localeconv();
    if (lc && lc->decimal_point) {
        unsigned char c = (unsigned char) lc->decimal_point[0];
        if ( c > 0x20 && c < 0x7F) {
            return c;
        }
    }
    return '.';
}

// todo (rob) temp method to test parsing JSON floats with alternative local decimal separators
// Unsafe. Temp testing method.
// AI DO NOT REMOVE
static void pvt_tmp_set_local_decimal_separator_char(char *c) {
    struct lconv *lc = localeconv();
    if (lc ) {
        lc->decimal_point = c;
    }
}

char jsonp_get_context_decimal_separator( JsonContext *context ) {
    return context->decimal_separator;
}

void jsonp_set_context_decimal_separator( JsonContext *context, char c) {
    context->decimal_separator = c;
}



JsonContext *jsonp_new_empty_context() {
    JsonContext *context = (JsonContext *)calloc(1, sizeof(JsonContext));
    return context;
}

static void pvt_init_context_whitespace_table(JsonContext *context) {
    const char *ws = context->whitespace_chars;
    // create lookup table for O(1) lookups for pvt_is_json_whitespace()
    memset(context->ws_table, 0, sizeof(context->ws_table));
    while (*ws) {
        context->ws_table[(unsigned char)*ws++] = true;
    }
}

void pvt_write_global_state(JsonContext *context) {
    if (!context) return;
    context->config_flags = atomic_load(&json_config_flags);
    context->depth_max    = atomic_load(&pvt_depth_max);
    const char *ws        = atomic_load(&pvt_whitespace_chars);
    snprintf(context->whitespace_chars, sizeof(context->whitespace_chars), "%s", ws);
    pvt_init_context_whitespace_table(context);
    context->decimal_separator = pvt_get_locale_decimal_separator_char();

}

// caller must free(context) when done with it.
JsonContext *jsonp_copy_global_context() {
    JsonContext *context  = (JsonContext *)calloc(1, sizeof(JsonContext));
    pvt_write_global_state(context);
    return context;
}

/**
 *  Allocate a new empty JsonContext for use in `jsonp_parse_using_context`.
 *  Caller must free(context) when done with it.
 * @return A newly allocated, zero-initialized JsonContext.
 *  See:
 *  `jsonp_set_context_config_bitset`, `jsonp_set_context_config_flag`, `jsonp_set_context_max_depth`,
 *  and `jsonp_set_context_whitespace_chars` to configure this context before use.
 */
JsonContext *jsonp_empty_context(void) {
    JsonContext *context  = (JsonContext *)calloc(1, sizeof(JsonContext));
    context->whitespace_chars[0] = '\0'; // empty string
    return context;
}


// reset to initial states all state-related members of the context.
// Does not affect depth_max, config_flags, whitespace_chars, or ws_table.
static void pvt_reset_context(JsonContext *context) {
    context->current_ptr    = nullptr;
    context->json_text      = nullptr;
    context->current_index  = 0;
    context->line           = 0;
    context->column         = 0;
    context->parse_start    = 0;
    context->parse_end      = 0;
    context->depth_current  = 0;
    memset(context->error_msg, '\0', ERROR_MSG_BUFFER_SIZE + 1 );
}

JsonValue *jsonp_parse_using_context(const char *json_text, JsonParseError *error, Arena *arena, JsonContext *context ) {
    pvt_reset_context(context);
    context->current_ptr = json_text;
    context->json_text = json_text;
    return pvt_jsonp_parse_impl(context, json_text, error, arena);
}

JsonValue *jsonp_parse(const char *json_text, JsonParseError *error, Arena *arena) {
    JsonContext context = {};
    pvt_write_global_state(&context);
    context.current_ptr = json_text;
    context.json_text = json_text;

    JsonValue *value = pvt_jsonp_parse_impl(&context, json_text, error, arena);
    return value;
}

JsonValue *jsonp_parse_ex(const char *json_text, JsonParseError *error, Arena *arena, const uint32_t buffer_size) {
    JsonContext context = {};
    pvt_write_global_state(&context);
    context.current_ptr = json_text;
    context.json_text = json_text;

    JsonValue *value = pvt_jsonp_parse_impl(&context, json_text, error, arena);
    if (!value) return nullptr;

    if (context.current_index < buffer_size) {
        snprintf(context.error_msg, ERROR_MSG_BUFFER_SIZE,
    "JSON text was successfully parsed, but characters remain in the buffer."
    " This can happen when the text is followed by embedded NUL characters.\nbuffer size=%u, bytes parsed=%u",
    buffer_size, context.current_index );
        context.parse_end = context.current_index;
        pvt_record_error(&context, error, JSON_ERR_UNEXPECTED_EOF, context.error_msg);
        return nullptr;

    }
    return value;
}

// -----------------------------------------------------------------
//      INITIALIZE
// -----------------------------------------------------------------

static _Atomic(bool) is_initialized = false;

// Canonical initializer
// Note, to parse Unicode and UTF-8 correctly, the caller should have called `setlocale` before here
// with an utf-8 locale.
// E.g., `setlocale(LC_ALL, "en_US.UTF-8");`
Error jsonp_init_3(jp_bitset_t config_flags, uint32_t max_depth, char const * whitespace_chars) {


    //todo temp debug remove
    pvt_tmp_set_local_decimal_separator_char(",");


    if (atomic_load(&is_initialized)) {
        return (Error){};  // already initialized, no-op, empty error
    }

    // Initialize Whitespace (Copy the argument)
    // ReSharper disable once CppDFAMemoryLeak
    char *default_ws = (char *)malloc(strlen(whitespace_chars) + 1);
    if (default_ws) {
        strcpy(default_ws, whitespace_chars);
        atomic_store(&pvt_whitespace_chars, default_ws);
    } else {
        return (Error){ .err = true, .reported_err = ENOMEM };
    }

    atomic_store(&json_config_flags, config_flags);
    atomic_store(&pvt_depth_max, max_depth);

    atomic_store(&is_initialized, true);
    // ReSharper disable once CppDFAMemoryLeak
    return (Error){};
}

Error jsonp_init_2(jp_bitset_t config_flags, uint32_t max_depth) {
    return jsonp_init_3(config_flags, max_depth, JSON_WHITESPACE_CHARS_DEFAULT);
}

Error jsonp_init_1(jp_bitset_t config_flags) {
    return jsonp_init_3( config_flags, JSON_DEPTH_MAX_DEFAULT, JSON_WHITESPACE_CHARS_DEFAULT);
}


Error jsonp_init() {
    return jsonp_init_3(JSON_CONFIG_FLAGS_DEFAULT, JSON_DEPTH_MAX_DEFAULT, JSON_WHITESPACE_CHARS_DEFAULT);
}


// -----------------------------------------------------------------
//      DESTROY
// -----------------------------------------------------------------

void jsonp_destroy(void) {
    if (atomic_exchange(&is_initialized, false)) {
        const char *ws = atomic_exchange(&pvt_whitespace_chars, nullptr);
        if (ws) {
            free( (void*)ws);
        }
    }
}



// -----------------------------------------------------------------
//      Pretty-Printer like output
// -----------------------------------------------------------------

static void pvt_json_object_str(JsonValue *object) {
    if (!object || object->type != JSON_OBJECT) return;
    if (object->u.object.count == 0 ) {
        printf("{ }");
        return;
    }
    // First entry
    printf("{ ");
    printf("'%s' : ",object->u.object.entries[0]->key);
    jsonp_print_json_value(object->u.object.entries[0]->value);
    for (size_t i = 1; i < object->u.object.count; ++i) {
        printf(", '%s' : ",object->u.object.entries[i]->key);
        jsonp_print_json_value(object->u.object.entries[i]->value);
    }
    printf(" }");
}

void json_array_str(JsonValue *array) {
    if (!array || array->type != JSON_ARRAY) return;
    if (array->u.array.count == 0 ) {
        printf("[ ]");
        return;
    }
    printf("[ ");
    jsonp_print_json_value(array->u.array.elements[0]);
    for (size_t i = 1; i < array->u.array.count; ++i) {
        printf(", ");
        jsonp_print_json_value(array->u.array.elements[i]);
    }
    printf(" ]");
}

void jsonp_print_json_value(JsonValue *value) {
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
        case JSON_LONG:
            printf("%ld", value->u.n_long);
            break;
        case JSON_LONG_LONG:
            printf("%lld", value->u.n_long_long);
            break;
        case JSON_DOUBLE:
            printf("%g", value->u.n_double);
            break;
        case JSON_LONG_DOUBLE:
            printf("%Lg", value->u.n_long_double);
            break;
        case JSON_STRING:
            printf("'%s'", value->u.string);
            break;
        case JSON_ARRAY:
            json_array_str(value);
            break;
        case JSON_OBJECT:
            pvt_json_object_str(value);
            break;
    }
}

const char *jsonp_parse_error_type_name(const JsonParseErrType err_type) {
    switch (err_type) {
        /* 3. Expand the list to create the Switch Cases */
#define X(name) case JSON_ERR_##name: return #name;
        JSON_ERROR_LIST(X)
#undef X
    }
    return "UNKNOWN_JSON_ERROR";
    /*switch (err_type) {
#define STR(x) case x: return #x
        STR(JSON_ERR_NONE);
        STR(JSON_ERR_NULL_TEXT);
        STR(JSON_ERR_EMPTY_TEXT);
        STR(JSON_ERR_UNEXPECTED_TEXT);
        STR(JSON_ERR_UNESCAPED_CONTROL_CHAR);
        STR(JSON_ERR_UNEXPECTED_EOF);
        STR(JSON_ERR_UNTERMINATED_ARRAY);
        STR(JSON_ERR_UNTERMINATED_STRING);
        STR(JSON_ERR_UNTERMINATED_OBJECT);
        STR(JSON_ERR_MISSING_COMMA);
        STR(JSON_ERR_MISSING_COLON);
        STR(JSON_ERR_INVALID_NUMBER_FORMAT);
        STR(JSON_ERR_INVALID_ESCAPE_SEQUENCE);
        STR(JSON_ERR_INVALID_UNICODE_ESCAPE);
        STR(JSON_ERR_NO_PRECEDING_HIGH_SURROGATE);
        STR(JSON_ERR_NO_FOLLOWING_LOW_SURROGATE);
        STR(JSON_ERR_RESERVED_FOR_HIGH_SURROGATE);
        STR(JSON_ERR_RESERVED_FOR_LOW_SURROGATE);
        STR(JSON_ERR_CODEPOINT_OUT_OF_RANGE);
        STR(JSON_ERR_INVALID_UTF8_START_BYTE);
        STR(JSON_ERR_INVALID_UTF8_CONTINUATION_BYTE);
        STR(JSON_ERR_OVERLONG_SEQUENCE);
        STR(JSON_ERR_MAX_NESTED_DEPTH_EXCEEDED);
        STR(JSON_ERR_UNEXPECTED_UTF16_ENCODING);
        STR(JSON_ERR_UNEXPECTED_UTF32_ENCODING);
        STR(JSON_ERR_BOM_NOT_ALLOWED);
        STR(JSON_ERR_TRAILING_COMMA_NOT_ALLOWED);
        STR(JSON_ERR_OUT_OF_MEMORY);
        STR(JSON_ERR_COUNT);
#undef STR
    }
    return "UNKNOWN_JSON_ERROR";*/
}

void jsonp_print_parse_error(JsonParseError *err) {
    if (!err) {
        printf("(JsonParseError)null\n");
        return;
    }
    // printf("%d:%s  line:%d col:%d pos:%d :  %s\n",
    //     err->err_type, jsonp_parse_error_type_name(err->err_type),
    //     err->line+1, err->column+1, err->first_bad_char,  err->message);

    //todo (rob) temp debug
    printf("%d:%s  line:%d col:%d pos:%d start:%d end:%d :  %s\n",
        err->err_type, jsonp_parse_error_type_name(err->err_type),
        err->line+1, err->column+1, err->first_bad_char, err->parse_start, err->parse_end, err->message);


    uint32_t err_pos = err->first_bad_char;
    // if (err_pos < 80) {
    //     // just print the first 80 characters of the JSON text
    //     printf("%s\n", err->json);
    //     return;
    // }

    // truncate around the err_pos, ideally 40 chars in each direction
    // todo (rob) we have to count Unicode code points for this!!
    // since 40 is the ideal midpoint, find out how many characters exist before err_pos
    uint32_t start = err_pos < 40 ? 0 : err_pos - 40;

    uint32_t chars_after_current = 0;
    char const * end_ptr = err->json + err_pos;
    while (*end_ptr++ != '\0' && chars_after_current < 40 ) {
        chars_after_current++;
    }

    uint32_t end = err_pos + chars_after_current - 1; // should be the range of characters we want to print
    uint32_t chars_to_display = end - start + 1;
    StringBuilder sb = {};
    sb_init( &sb, chars_to_display, "");
    int chars_written = snprintf(sb.buffer, chars_to_display + 1, "%.*s", chars_to_display+1, err->json + start);
    if ( chars_written <= 0) {
        fprintf(stderr, "jsonp_print_parse_error: snprintf failed with return code: %d, chars_to_display:%d\n",
            chars_written, chars_to_display);
        fprintf(stderr, "json_text:%s\n", err->json);

        fflush(stderr);
        fflush(stdout);
        sb_destroy(&sb);
        return;
    }

    sb.length = chars_written;
    sb.buffer[chars_written] = '\0';

    // printf("chars_written: %d\n", chars_written);

    // printf("%.*s\n", chars_to_display, err->json + start);
    // "\n\r\t\f\v"
    uint32_t num_replacements = sb_replace_match_chars(&sb, "\n\r\t\f\v", ' ');
    // printf("number_replacements:%u\n", num_replacements);

    char const *caret = "\033[91m^\033[0m";
    char const *left_caret = "\033[91m>\033[0m";
    char const *right_caret = "\033[91m<\033[0m";

    uint32_t line_err_index = err_pos - start;

    // insert right caret
    if (line_err_index < (uint32_t)chars_written) {
        sb_insert_str(&sb, right_caret, line_err_index + 1);
    } else {
        sb_append_str(&sb, right_caret);
    }
    // insert left caret
    sb_insert_str(&sb, left_caret, line_err_index);

    printf("%s\n", sb.buffer);

    // prints caret on following line:
    /*for (uint32_t i = 0; i < line_err_index ; ++i) {
        putchar(' ');
    }
    // printf("^");
    printf("%s", caret);
    printf("\n");*/

    sb_destroy(&sb);
}

//// ------------------------------------------------------------
////
////    TESTING
////
//// ------------------------------------------------------------


void parse_test_str(char const * str) {
    Error init_err = jsonp_init();
    if (init_err.err) {
        err_print(init_err);
        jsonp_destroy();
        return;
    }
    Arena arena = {};
    ArenaErrResult aer = arena_create_arena( &arena, ONE_MIBIBYTE * 100);
    if ( aer.err ) {
        printf("arena_create_arena failed with %d, %s\n", aer.reported_err, aer.msg);
        jsonp_destroy();
        return;
    }
    JsonParseError err = {.json = str};
    printf("\nParsing json string '%s': \n", str);
    JsonValue *jval = jsonp_parse(str, &err, &arena);
    if (!jval) {
        printf("ERROR %d: first_bad_char:%d, line:%d col:%d start:%d end:%d  %s\n",
           err.err_type, err.first_bad_char,  err.line, err.column, err.parse_start, err.parse_end, err.message);
    }
    else {
        jsonp_print_json_value(jval);
        printf("\n");
    }

    arena_destroy_arena(&arena);
    jsonp_destroy();
}

void parse_test_str_custom_init(
        char const * str,
        jp_bitset_t config_flags,
        uint32_t max_depth,
        char const * whitespace_chars )
{

    if (! whitespace_chars ) whitespace_chars = JSON_WHITESPACE_CHARS_DEFAULT;
    Error err = jsonp_init_3( config_flags, max_depth,whitespace_chars );
    if (err.err) {
        printf("reported error: %d, message: %s\n", err.reported_err, err.msg);
    }

    Arena arena = {};
    ArenaErrResult aer = arena_create_arena( &arena, ONE_MIBIBYTE * 100);
    if ( aer.err ) {
        printf("arena_create_arena failed with %d, %s\n", aer.reported_err, aer.msg);
    }
    JsonParseError json_err = {};
    printf("\nParsing json string '%s': \n", str);
    JsonValue *jval = jsonp_parse(str, &json_err, &arena);
    if (!jval) {
        printf("ERROR %d: first_bad_char:%d, line:%d col:%d start:%d end:%d  %s\n",
           json_err.err_type, json_err.first_bad_char,  json_err.line, json_err.column, json_err.parse_start,
           json_err.parse_end, json_err.message);
    }
    else {
        jsonp_print_json_value(jval);
        printf("\n");
    }

    arena_destroy_arena(&arena);
    jsonp_destroy();

}


void simple_parse(char const *json_text) {

    Error init_err = jsonp_init();
    if (init_err.err) {
        err_print(init_err);
        jsonp_destroy();
        return;
    }

    Arena arena = {};
    ArenaErrResult aer = arena_create_arena( &arena, 1024 * 124);  // initially 1MB as an example. Grows as needed.
    if ( aer.err ) {
        printf("arena_create_arena failed with %d, %s\n", aer.reported_err, aer.msg);
        jsonp_destroy();
        return;
    }

    JsonParseError err = {};

    JsonValue *jval = jsonp_parse(json_text, &err, &arena);
    if (!jval) {
        // handle error
        jsonp_print_parse_error(&err);
    } else {
        jsonp_print_json_value(jval);
        putchar('\n');
    }

    arena_destroy_arena(&arena);
    jsonp_destroy();
}

void test_custom_flags(void) {
    jp_bitset_t my_custom_flags =
        jsonp_make_config_flag_bitset( 3, (JsonConfigFlag[3]) {
            JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_ARRAYS,
            JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_OBJECTS,
            JSON_CONFIG_ALLOW_UNICODE_U_ESCAPE
            });

    // first should error:
    parse_test_str_custom_init("[ 1,2, ]", 0, JSON_DEPTH_MAX_DEFAULT, JSON_WHITESPACE_CHARS_DEFAULT);
    // this should pass
    parse_test_str_custom_init("[ 1,2, ]", my_custom_flags, JSON_DEPTH_MAX_DEFAULT, JSON_WHITESPACE_CHARS_DEFAULT);

    // first should error:
    parse_test_str_custom_init("{ \"one\":1, \"two\":2, }", 0, JSON_DEPTH_MAX_DEFAULT, JSON_WHITESPACE_CHARS_DEFAULT);
    // this should pass
    parse_test_str_custom_init("{ \"one\":1, \"two\":2, }", my_custom_flags, JSON_DEPTH_MAX_DEFAULT, JSON_WHITESPACE_CHARS_DEFAULT);

    // first should error:
    parse_test_str_custom_init("\"one:\\U012348\"", 0, JSON_DEPTH_MAX_DEFAULT, JSON_WHITESPACE_CHARS_DEFAULT);
    // this should pass
    parse_test_str_custom_init("\"one:\\U012348\"", my_custom_flags, JSON_DEPTH_MAX_DEFAULT, JSON_WHITESPACE_CHARS_DEFAULT);
}


void test_multi_byte_char_strings() {
    // one byte
    parse_test_str("\"\x41\"");     // A

    // two bytes
    parse_test_str("\"\xC1\x80\""); // invalid
    parse_test_str("\"\xC2\x80\""); // ''

    // three bytes
    parse_test_str("\"\xE0\xA0\x80\""); // 'ࠀ'
    parse_test_str("\"\xE1\x95\xBB\""); // 'ᕻ'
    parse_test_str("\"\xED\x95\xBB\""); // '핻'
    parse_test_str("\"\xEE\x95\xBB\""); // ''
    parse_test_str("\"\xEF\xBF\xBF\""); // '￿'

    // four bytes
    parse_test_str("\"\xF0\x90\x80\x80\""); // '𐀀'
    parse_test_str("\"\xF0\x9F\x98\x80\""); // '😀'
    parse_test_str("\"\xF0\xBF\xBF\xBF\""); // '𿿿'

    parse_test_str("\"\xF1\x80\x80\x80\""); // '𿿿'
    parse_test_str("\"\xF3\xBF\xBF\xBF\""); // '󿿿'

    parse_test_str("\"\xF4\x80\x80\x80\""); // '󿿿'
    parse_test_str("\"\xF4\x8F\xBF\xBF\""); // '󿿿'

    parse_test_str("\"\xF4\x90\x80\x80\""); // '󿿿'


}

void test_parse_unicode_escapes() {
    // parse_test_str("\"\\uCAFE \\uBABE\"");
    // parse_test_str("\"\\uCAFE\\uBABE\"");

    // parse_test_str("\"\\uD801\\uDC01\"");   // valid high- / low-surrogate pair=
    //
    // parse_test_str("\"\\uDC01\"");      //low surrogate without preceding high surrogate
    // parse_test_str("\"\\uD801\"");      //high surrogate without following low surrogate
    //
    // parse_test_str("\"\\uDC01\\uDC02\"");      //two low surrogates in a row
    // parse_test_str("\"\\uD801\\uD802\"");      //two high surrogates in a row


    parse_test_str("\"\\u0041\""); // A
    parse_test_str("\"\\u0080\"");  // ''

    parse_test_str("\"\\u0800\"");  // 'ࠀ'

    parse_test_str("\"\\uD834\\uDD1E\"");

    parse_test_str("\"😀  \\uD83D\\uDE00\"");

    parse_test_str("\"😀  \\U01F600\"");  // Our custom enhancement! Allows full unicode codepoint without surrogates


    // the code that renders glyphs combines them into one glyph!!
    parse_test_str("\" combining character: C with tail: \\u0043\\u0327\"");

    // Test: Combining character vs Precomposed
    // \u00E9 is 'é' (1 codepoint)
    // e\u0301 is 'e' + '´' (2 codepoints)
    printf("\n--- Combining Character Test ---\n");
    parse_test_str("\"\\u00E9\"");
    parse_test_str("\"e\\u0301\"");
    printf("Note: Both should look identical in the terminal, but 'raw bytes' count will differ.\n");

    // parse_test_str("\"\\uABCDAPPLE\"");
}

void test_null_parse(void) {
    parse_test_str("null");
    parse_test_str(" null ");
    parse_test_str("nul");
    parse_test_str("nu");
    parse_test_str("n");

    parse_test_str("number");
    parse_test_str("next");

    parse_test_str("nulll");
}

void test_true_parse(void) {
    parse_test_str("true");
    parse_test_str(" true ");
    parse_test_str(" truetrue ");
    parse_test_str(" true true");
    parse_test_str(" tr true");
}

void test_false_parse(void) {
    parse_test_str("false");
    parse_test_str(" false ");
    parse_test_str(" falsefalse ");
    parse_test_str(" false false");
    parse_test_str(" fals ");
}

void test_number_parse(void) {
    // parse_test_str("\\x43");
    // parse_test_str("-");
    // parse_test_str("-A");

    // parse_test_str("-0");
    // parse_test_str("0");
    // parse_test_str("0a");
    parse_test_str("1");
    parse_test_str("-22");
    parse_test_str("333");
    parse_test_str("-4444");
    parse_test_str("55555");
    parse_test_str("-666666");
    parse_test_str("7777777");
    parse_test_str("-88888888");
    parse_test_str("999999999");

    // parse_test_str(".");
    // parse_test_str("0.");
    // parse_test_str("-0.");
    // parse_test_str("1.");
    // parse_test_str("-2.");
    // parse_test_str(".1");
    // parse_test_str("-.22");

    parse_test_str("1.1");
    parse_test_str("-3.3");
    parse_test_str("-3.3e24");
    parse_test_str("5.3e-24");
    parse_test_str("5.67 moo");
    parse_test_str("[100000000000000000000]");
    parse_test_str("[123123e100000]");
    parse_test_str("[-123123e100000]");
    parse_test_str("[0.4e00669999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999969999999006]");
    parse_test_str("[0.4e-00669999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999969999999006]");
    parse_test_str("[-0.4e-00669999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999969999999006]");

    parse_test_str("[-123123123123123123123123123123]");

    // parse_test_str("[2.e3]");
    // parse_test_str("[0.1.2]");
    // parse_test_str("[1,]");

}

void test_array_parse(void) {
    // parse_test_str("[]");
    // parse_test_str("[1]");
    // parse_test_str("[1, 2]");
    // parse_test_str("[1 2]");
    //
    // parse_test_str("[1, 2, true, false, null, 3.33, 4e20]");
    //
    // parse_test_str("[ 1, 2]]");

    // parse_test_str("[ 1, ");
//[ [1,2,3], [4,5,6], [true,false,null] ]
    parse_test_str("[[1,2 ] ] ");
    parse_test_str("[[1 ], [2] ] ");
    parse_test_str("[[1,2] ] ");
    parse_test_str("[[1,2],[3,4]] ");
    parse_test_str("[ [1,2],[3,4]] ");
    parse_test_str("[ [1,2], [3,4]] ");
    parse_test_str("[ [1,2], [3,4] ] ");

    parse_test_str("[ [1,2,3], [4,5,6], [true,false,null] ] ");

}

void test_parse_objects(void ) {
    parse_test_str("{}");   // empty object is fine by the spec

    parse_test_str("{ \"foo\": null }");

    parse_test_str("{ \"one\": \"one\", \"two\" : 2, \"three\" : 3.33 }   ");

    parse_test_str("{ \"one\": \"one\", \"two\" : 2, \"three\" : 3.33, \"null\" : null, \"true\":true, \"false\":false }");

    parse_test_str(" [{ \"name\": \"jelly bowl\", \"ff\": 5}, {\"name\": \"werewolf\", \"ff\": 10 }]");
}




// Helper to see exactly what bytes are in a string
static void debug_dump_bytes(const char *label, const char *s) {
    printf("%-25s: ", label);
    while (s && *s) {
        printf("%02X ", (unsigned char)*s++);
    }
    printf("\n");
}

static void test_hex_and_chars(void) {
    char const * str = "Apôñéas\u253C\U0001F604\U0001F64F";
    // 1. Output to Console
    printf("Console Output:\n");
    Writer out = writer_to_file(stdout);
    repr_hex_and_chars(&out, str);
    repr_hex_and_chars_for_codepoint(&out, 0x01F64F);

    printf("\nhigh surrogate range:\nstart: ");
    repr_hex_and_chars_for_codepoint(&out, 0xD800);
    printf("  end: ");
    repr_hex_and_chars_for_codepoint(&out, 0xDBFF);
    printf("low surrogate range:\nstart: ");
    repr_hex_and_chars_for_codepoint(&out, 0xDC00);
    printf("  end: ");
    repr_hex_and_chars_for_codepoint(&out, 0xDFFF);
}

void test_indeterminates(void) {
    // parse_test_str("[\"日ш�\"]");

    // a really big int.
    parse_test_str("[100000000000000000000]");  // parses as [ 1e+20 ]


    // + overflow
    parse_test_str("[123123e100000]"); // [ inf ]

    // - overflow
    parse_test_str("[-123123e100000]");  // [ -inf ]

    // huge exp- parses as [ inf ]
    parse_test_str("[0.4e00669999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999969999999006]");

    // too big negative int
    parse_test_str("[-123123123123123123123123123123]");  // [ -1.23123e+29 ]

    // number_double_huge_neg_exp.json
    parse_test_str("[123.456e-789]");  // [ 0 ]

    //number_pos_double_huge_exp.json
    parse_test_str("[1.5e+9999]"); // [ inf ]

    //number_real_underflow.json
    parse_test_str("[123e-10000000]");  // [ 0 ]

    // number_very_big_negative_int.json  - parses a [ -2.37462e+47 ][ -2.37462e+47 ]
    parse_test_str("[-237462374673276894279832749832423479823246327846]");

    // number_neg_int_huge_exp.json
    parse_test_str("[-1e+9999]");  // [ -inf ]
}

void test_json_test_suite_fails(void) {
    simple_parse("{]");
}

void test_fails_for_reporting(void) {

    // printf("unterminated string\033[91m^\033[0m\033[91m^\033[0m\n");


    printf("Output:\n");
    simple_parse("1");
    simple_parse("[\"string\", -2, 3.2, -4e-2, true, false, null ]");
    simple_parse(
        "{ \"nested array\": [ \"item 1\", \"item 2\", \"item 3\" ], "
                  "\"nested object\": { \"key 1\": \"value 1\", \"key 2\": \"value 2\", \"key 3\": \"value 3\" } }");

    // bad JSON text
    printf("\nfailing parses:\n");

    simple_parse("\"unterminated string ");
    simple_parse("[ \"unterminated array\", 2 ");
    simple_parse("[ \"trailing comma\", 2,] ");
    //
    // char const *big_str = " [\n"
    // "   { \"id\":  0,  \"name\": \"NULL ROOM\",         \"desc\":  \"\" }, \n"
    // "   { \"id\":  1,  \"name\": \"Battlements\",       \"desc\":  \"You are out on the battlements of the Chateau. There is only one way back.\" }, \n"
    // "   { \"id\":  2,  \"name\": \"Magician's Room\",   \"desc\":  \"This is an eerie room, where once magicians consorted with evil sprites and werebeasts. Exits lead in three directions. An evil smell comes from the south.\" }, \n"
    // "   { \"id\":  3,  \"name\": \"Straw Mattress\",    \"desc\":  \"An old straw mattress lies in one corner. It has been ripped apart to find any treasure which was hidden in it. Light comes fitfully from a window to the north, and around the doors to south, east, and west.\" }, \n"
    // "   { \"id\":  4,  \"name\": \"Wooden Panels\",     \"desc\":  \"This wooden-panelled room makes you feel damp and uncomfortable. There are three doors leading from this room, one made of iron. Your sixth sense warns you to choose carefully...\" }, \n"
    // "   { \"id\":  5,  \"name\": \"Living Stone\",      \"desc\":  \"You ignore your intuition... A Spell of Living Stone, primed to trap the first intruder has been set on you. With your last seconds of life you have time only to feel profound regret...\" }, \n"
    // "   { \"id\":  6,  \"name\": \"L-Shaped Room\",     \"desc\":  \"You are in an L-shaped room. Heavy parchment lines the walls. You can see through an archway to the east, but that is not the only exit from this room.\" }, \n"
    // "   { \"id\":  7,  \"name\": \"Archway\",           \"desc\":  \"There is an archway to the west, leading to an L-shaped room. A door leads in the opposite direction.\" }, \n"
    // "   { \"id\":  8,  \"name\": \"Kitchen\",           \"desc\":  \"This must be the Chateau's main kitchen, but any food left here has long rotted away. A door leads to the north, and there is one to the west.\" }, \n"
    // "   { \"id\":  9,  \"name\": \"Black Dragon\",      \"desc\":  \"You find yourself in a small room, which makes you feel claustrophobic. There is a picture of a black dragon painted on the north wall, above the door.\" }, \n"
    // "   { \"id\": 10,  \"name\": \"Landing\",           \"desc\":  \"A stairwell ends in this 'room', which is more of a landing than an actual room. The door to the north is made of iron, which has rusted over the centuries.\" }, \n"
    // "   { \"id\": 11,  \"name\": \"Stone Archway\",     \"desc\":  \"There is a stone archway to the north. You are in a very long room.\\nFresh air blows down some stairs and rich red drapes cover the walls. You can see doors to the east.\" }, \n"
    // "   { \"id\": 12,  \"name\": \"Whirling Smoke\",    \"desc\":  \"You have entered a room filled with swirling, choking smoke. You must leave quickly to remain healthy enough to continue your chosen quest.\" }, \n"
    // "   { \"id\": 13,  \"name\": \"Charism Reduction\", \"desc\":  \"There is a mirror in the corner. You glance at it, and feel suddenly very ill.\\nYou realize the looking-glass has been infused with a Spell of Charisma Reduction... oh dear....\" }, \n"
    // "   { \"id\": 14,  \"name\": \"White Marble\",      \"desc\":  \"This room is richly finished with a white marble floor. Strange footprints lead to the two doors from this room. Dare you follow them?\" }, \n"
    // "   { \"id\": 15,  \"name\": \"Red Drapes\",        \"desc\":  \"You are in a long, long hallway, lined on each side with rich, red drapes.\\nThey are parted halfway down the east wall where there is a door.\" }, \n"
    // "   { \"id\": 16,  \"name\": \"Yellow Room\",       \"desc\":  \"Someone has spent a long time painting this room a bright yellow.\\nYou remember reading that yellow is the Ancient Oracle's Color of Warning...\" }, \n"
    // "   { \"id\": 17,  \"name\": \"Ladder\",            \"desc\":  \"As you stumble down the ladder you fall into the room. The ladder crashes down behind you. There is now no way back.\\nA small door leads east from this very cramped room.\" }, \n"
    // "   { \"id\": 18,  \"name\": \"Hall of Mirrors\",   \"desc\":  \"You find yourself in the Hall of Mirrors, and see yourself reflected a hundred times or more. Through the bright glare you can make out doors in all directions. You notice the mirrors around the east door are heavily tarnished.\" }, \n"
    // " ] ";
    //
    //
    // simple_parse(big_str);
}

#ifdef JSON_PARSER_2_MAIN
int main( ) {
    // Set locale to ensure printf doesn't mangle UTF-8 bytes based on system defaults
    if (!setlocale(LC_ALL, "en_US.UTF-8")) {
        setlocale(LC_ALL, ""); // Fallback to system default locale
    }
    //
    // printf("--- Encoding Debug ---\n");
    //
    // // Simplified output to reduce terminal rendering confusion
    // printf("UCN: '%s'\n", "\U00010401");
    // printf("u8:  '%s'\n", (const char*)u8"\U00010401");
    // printf("HEX: '%s'\n", "\xF0\x90\x90\x81");
    //
    // // Print isolated to test terminal font fallback
    // printf("Isolated: ");
    // printf("\U00010401");
    // printf("\n");
    // fflush(stdout);

    // printf("Pound sign: %s\n", "");
    // printf("\\u0800: \u0800\n");
    // printf("\\U+0001f600: \U0001f600\n");

    // test_null_parse();
    // test_true_parse();
    // test_false_parse();
    test_number_parse();
    // test_array_parse();
    // test_parse_objects();
    // test_parse_unicode_escapes();

    // test_multi_byte_char_strings();

    // test_hex_and_chars();

    // test_indeterminates();

    // test_custom_flags();

    // test_fails_for_reporting();
    // test_json_test_suite_fails();


}
#endif
