//  json_parser_2.c
// Created by Rob Ross on 7/2/26.
//
// Migrating and reworking the existing json_parser.c here, which will replace it when completed


#include "roblib/json_parser.h"
#include <ctype.h>

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "roblib/string_builder.h"
#include "roblib/unicode_tools.h"

/*
 *  todo - configuration flags
 *  4. treat -0 as float or int by default?
 *  8. Parse -0 as int or float?
 */



static uint64_t json_config_flags = 0;

bool jsonp_is_flag_set(JsonConfigFlag flag) {
    return json_config_flags & ( 1 << flag) ;
}

void jsonp_set_config_flag(JsonConfigFlag flag) {
    json_config_flags = json_config_flags | ( 1 << flag);
}

void jsonp_clear_config_flag(JsonConfigFlag flag) {
    json_config_flags = json_config_flags & ~( 1 << flag);
}

typedef struct json_context_t {
    const char *current_ptr;  // The current text being parsed, advances through the JSON text in the json member
    const char *json; // full original JSON text string
    uint32_t current_index; // the index of the character the lexer is scanning
    uint32_t line;
    uint32_t column;
    uint32_t parse_start;
    uint32_t parse_end;
} JsonContext;

// -----------------------------------------------------------------
//      WHITE SPACE
// -----------------------------------------------------------------

static char const * const WHITE_SPACE_CHARS_DEFAULT = "\x20\x09\x0A\x0D";

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
 * @param ws_chars the characters that should be treated as white space characters.
 *
 */
void jsonp_define_whitespace_chars(JsonContext *context, char const *ws_chars);

static inline bool pvt_is_json_whitespace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void pvt_skip_whitespace(JsonContext *context) {
    while (*context->current_ptr && pvt_is_json_whitespace((unsigned char)*context->current_ptr)) {
        // unsigned char c = (unsigned char)*context->current_ptr;
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

constexpr uint32_t ERROR_MSG_BUFFER_SIZE = 1023;
static char error_msg_buffer[ERROR_MSG_BUFFER_SIZE + 1] = {};

constexpr uint32_t DEPTH_MAX_DEFAULT = 64;
static uint32_t depth_max = DEPTH_MAX_DEFAULT; // todo (rob) make this Threadlocal
static uint32_t depth_current = 0;

// -----------------------------------------------------------------
//      BOM CONSTANTS
// -----------------------------------------------------------------

char const * const BOM_UTF8     = "\xEF\xBB\xBF";
char const * const BOM_UTF16_BE = "\xFE\xFF";
char const * const BOM_UTF16_LE = "\xFF\xFE";
char const * const BOM_UTF32_BE = "\x00\x00\xFE\xFF";
char const * const BOM_UTF32_LE = "\xFF\xFE\x00\x00";

/**
 *  Sets the maximum nesting depth allowed in the json text. If depth is exceeded, the json text is
 *  rejected as invalid.
 *  The default is specified in DEPTH_MAX_DEFAULT
 *
 */
void jsonp_set_max_nesting_depth(JsonContext *context, uint32_t depth);

// -----------------------------------------------------------------
//      Forward Declarations
// -----------------------------------------------------------------
static JsonValue * pvt_parse_value(JsonContext *context, JsonError *error, Arena *arena );
static JsonValue * pvt_parse_string(JsonContext *context, JsonError *error, Arena *arena );



// advance the parser state based on the current parse window
static void pvt_advance(JsonContext *context, const int char_count) {
    context->current_ptr    += char_count;
    context->current_index  += char_count;
    context->column         += char_count;
}

static void pvt_record_error(
    const JsonContext *context, JsonError *error, const enum json_error_type err_type, const char *msg) {

    error->message = msg;
    error->json = context->json;
    error->err_type = err_type;
    error->first_bad_char = context->current_index;
    error->line   = context->line;
    error->column = context->column;
    error->parse_start = context->parse_start;
    error->parse_end = context->parse_end;
}

static bool pvt_max_depth_exceeded(JsonContext *context, JsonError *error) {
    if (depth_current > depth_max) {
        snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE,
            "max nested depth of %d exceeded", depth_max);
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_MAX_NESTED_DEPTH_EXCEEDED, error_msg_buffer);
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

static JsonObjectEntry * pvt_parse_one_entry(JsonContext *context, JsonError *error, Arena *arena) {
    pvt_skip_whitespace(context);
    context->parse_start = context->current_index;  // need this here since we aren't calling pvt_parse_value()
    JsonValue *key = pvt_parse_string(context, error, arena);
    if (!key) {
        // error-out immediately
        return nullptr;
    }
    pvt_skip_whitespace(context);

    // need to parse a colon ":" here:
    if (*context->current_ptr != ':' ) {
        char const *msg = "expected name-separator ':'";
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_MISSING_COLON, msg);
        return nullptr;
    }
    pvt_advance(context, 1);  // consume ':'

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

static JsonValue * pvt_parse_object(JsonContext *context, JsonError *error, Arena *arena ) {
    // we recursively parse members of this object until we see end-of-object '}' char or run out of text
    // todo refactor to use stack DS and iterative algorithm instead of recursion
    depth_current++;
    if ( pvt_max_depth_exceeded(context, error)) {
        depth_current--;
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
    if ( *context->current_ptr && *context->current_ptr != '}') {
        JsonObjectEntry *joe = pvt_parse_one_entry(context, error, arena);
        if (!joe) {
            depth_current--;
            return nullptr;  // error-out immediately
        }
        entries = pvt_add_json_object_entry_node(entries, joe, arena);
        num_entries++;
        pvt_skip_whitespace(context);
    }


    while ( *context->current_ptr && *context->current_ptr != '}' ) {
        pvt_skip_whitespace(context);
        // expect ','
        if (*context->current_ptr != ',' ) {
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_MISSING_COMMA, "expected comma");
            depth_current--;
            return nullptr;
        }
        // comma is expected delimiter between object entries
        pvt_advance(context, 1);  // consume ','
        pvt_skip_whitespace(context);

        if (*context->current_ptr == '}') {
            // we have a comma without another entry
            if (jsonp_is_flag_set(JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_OBJECTS)) {
                // this allowed
                break;
            }
            // this is an error
            context->parse_start = context->current_index;
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_TRAILING_COMMA_NOT_ALLOWED, "trailing comma not allowed in object entry list");
            depth_current--;
            return nullptr;
        }

        JsonObjectEntry *joe = pvt_parse_one_entry(context, error, arena);
        if (!joe) {
            depth_current--;
            return nullptr;
        }
        entries = pvt_add_json_object_entry_node(entries, joe, arena);
        num_entries++;
    }

    if (*context->current_ptr != '}' ) {
        char const *msg = "unterminated object";
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_UNTERMINATED_OBJECT, msg);
        depth_current--;
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

    depth_current--;
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


static JsonValue * pvt_parse_array(JsonContext *context, JsonError *error, Arena *arena) {
    // we recursively parse elements of this array until we see end-of-array ']' char or run out of text
    // todo refactor to use stack DS and iterative algorithm instead of recursion
    depth_current++;
    if ( pvt_max_depth_exceeded(context, error)) {
        depth_current--;
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
    if ( *context->current_ptr && *context->current_ptr != ']') {
        JsonValue *value = pvt_parse_value(context, error, arena);
        if (!value) {
            context->parse_end = context->current_index;
            depth_current--;
            return nullptr;  // error-out immediately
        }
        elements = pvt_add_json_value_node(elements, value, arena);
        num_elements++;
        pvt_skip_whitespace(context);
    }

    while ( *context->current_ptr && *context->current_ptr != ']' ) {
        pvt_skip_whitespace(context);
        //expect ','
        if (*context->current_ptr != ',' ) {
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_MISSING_COMMA, "expected comma");
            depth_current--;
            return nullptr;
        }
        // comma is expected delimiter between array elements
        pvt_advance(context, 1);  // consume ','
        pvt_skip_whitespace(context);

        if (*context->current_ptr == ']') {
            // we have a comma without another value
            if (jsonp_is_flag_set(JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_ARRAYS)) {
                // this allowed
                break;
            }
            // this is an error
            context->parse_start = context->current_index;
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_TRAILING_COMMA_NOT_ALLOWED, "trailing comma not allowed in array element list");
            depth_current--;
            return nullptr;
        }

        JsonValue *value = pvt_parse_value(context, error, arena);
        if (!value) {
            depth_current--;
            return nullptr;
        }
        elements = pvt_add_json_value_node(elements, value, arena);
        num_elements++;
    }
    pvt_skip_whitespace(context);

    if (*context->current_ptr != ']' ) {
        char const *msg = "unterminated array";
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_UNTERMINATED_ARRAY, msg);
        depth_current--;
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

    depth_current--;
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
static uint32_t pvt_decode_utf8(const uint32_t num_bytes, uint8_t utf8_bytes[]) {
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
static StringBuilder * pvt_encode_utf8( JsonContext *context, JsonError *error, const uint32_t codepoint, StringBuilder *sb) {
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
            snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE,
        "'U+%.4X' is reserved as a high/leading surrogate and cannot be used as a codepoint.", codepoint);
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_RESERVED_FOR_HIGH_SURROGATE, error_msg_buffer);
            return nullptr;
        } else {
            // low/ trailing surrogate
            snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE,
        "'U+%.4X' is reserved as a low/trailing surrogate and cannot be used as a codepoint.", codepoint);
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_RESERVED_FOR_LOW_SURROGATE, error_msg_buffer);
            return nullptr;
        }
    }

    if (codepoint <= 127 ) {
        sb_append_char(sb, (uint8_t)codepoint);
    } else if (codepoint <= 0x07FF ) {
        // encode as two UTF-8 bytes
        //110xxxxx 10xxxxxx
        uint8_t first  = 0b11000000    | (uint8_t)( codepoint >> 6 );
        uint8_t second = continue_bits | (uint8_t)( codepoint & continue_mask );
        sb_append_char(sb, first);
        sb_append_char(sb, second);

    } else if (codepoint <= 0xFFFF) {
        // encode as three UTF-8 bytes
        // 1110xxxx 10xxxxxx 10xxxxxx
        uint8_t first  = 0b11100000    | (uint8_t)( codepoint >> 12 ) ;
        uint8_t second = continue_bits | (uint8_t)( codepoint >> 6 & continue_mask);
        uint8_t third  = continue_bits | (uint8_t)( codepoint      & continue_mask );
        sb_append_char(sb, first);
        sb_append_char(sb, second);
        sb_append_char(sb, third);

    } else if (codepoint <= 0x10FFFF) {
        // encode as four UTF-8 bytes
        // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        uint8_t first   = 0b11110000    | (uint8_t)( codepoint >> 18 ) ;
        uint8_t second  = continue_bits | (uint8_t)( codepoint >> 12 & continue_mask);
        uint8_t third   = continue_bits | (uint8_t)( codepoint >>  6 & continue_mask);
        uint8_t fourth  = continue_bits | (uint8_t)( codepoint       & continue_mask );
        sb_append_char(sb, first);
        sb_append_char(sb, second);
        sb_append_char(sb, third);
        sb_append_char(sb, fourth);

    } else {
        // error, codepoint out of range
        snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE,
    "'U+%.4X' is out of range and cannot be used as a codepoint.", codepoint);
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_CODEPOINT_OUT_OF_RANGE, error_msg_buffer);
        return nullptr;
    }

    return sb;
}

static uint32_t pvt_parse_hex_impl(JsonContext *context, JsonError *error, const uint32_t num_chars) {
    uint32_t result = 0;
    const char *json_ptr = context->current_ptr;
    for (uint32_t i = 0; i < num_chars; i++) {
        if (*json_ptr == '\0') {
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_UNEXPECTED_EOF, "Unexpected EOF while parsing hex digit.");
            //  clang/clion linter doesn't see that record_error changes err_type.
            //   without this, it erroneously reports of unreachable code in calling methods
            error->err_type = JSON_ERR_UNEXPECTED_EOF;
            return 0;
        }
        if (!isxdigit((unsigned char)*json_ptr)) {
            context->parse_end = context->current_index;
            snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE, "Invalid hex digit in Unicode escape: %c", *json_ptr);
            pvt_record_error(context, error, JSON_ERR_INVALID_UNICODE_ESCAPE, error_msg_buffer);
            return 0;
        }

        // convert hex digit to uint and shift into result.
        const uint32_t dec_value = (uint32_t)pvt_hex_to_dec(*json_ptr);  // dec_value: 0 - F
        result = result * 16 | dec_value;
        json_ptr++;
    }
    pvt_advance(context, num_chars);
    return result;
}

// try to parse 6 hex bytes from the stream. Report in error if we didn't find 6 hex bytes.
static uint32_t pvt_parse_hex6(JsonContext *context, JsonError *error) {
    return pvt_parse_hex_impl(context, error, 6);
}

// try to parse 4 hex bytes from the stream. Report in error if we didn't find 4 hex bytes.
static uint16_t pvt_parse_hex4(JsonContext *context, JsonError *error) {
    return pvt_parse_hex_impl(context, error, 4);
}

//  expect a valid continuation byte from 0x80 - 0xBF at current index
// Return true if found, otherwise return false and report error
static bool pvt_validate_utf8_continuation_byte(
    JsonContext *context, JsonError *error,
    uint8_t current_byte, uint8_t start_range, uint8_t end_range) {

    uint8_t next_byte = *context->current_ptr;

    if ( !( next_byte >= start_range && next_byte <= end_range )) {
        snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE,
    "'0x%.2X' is an invalid UTF-8 continuation byte after '0x%.2X'", next_byte, current_byte);
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_INVALID_UTF8_CONTINUATION_BYTE, error_msg_buffer);
        return false;
    }

    return true;
}

static bool pvt_validate_utf8(JsonContext *context, JsonError *error,  StringBuilder *sb) {
    uint8_t stream_bytes[4] = {};
    uint8_t lead_byte = *context->current_ptr;;
    uint32_t num_bytes = 0;
    stream_bytes[num_bytes++] = lead_byte;

    // -----------------------------------------------------------------
    //          ONE BYTE (ASCII)
    // -----------------------------------------------------------------

    if (lead_byte <= 127 ) {
        sb_append_char(sb, lead_byte);
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
        current_byte = (uint8_t)*context->current_ptr;
        stream_bytes[num_bytes++] = current_byte;
        pvt_advance(context, 1);
        //happy path
        sb_append_char(sb, stream_bytes[0]);
        sb_append_char(sb, stream_bytes[1]);
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
            current_byte = (uint8_t)*context->current_ptr;
            if ( current_byte >= 0x80 && current_byte <= 0x9F) {
                snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE,
            "'0x%.2X' is an invalid UTF-8 continuation byte and overlong sequence.", current_byte);
                context->parse_end = context->current_index;
                pvt_record_error(context, error, JSON_ERR_OVERLONG_SEQUENCE, error_msg_buffer);
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
            current_byte = (uint8_t)*context->current_ptr;
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            //happy path
            sb_append_char(sb, stream_bytes[0]);
            sb_append_char(sb, stream_bytes[1]);
            sb_append_char(sb, stream_bytes[2]);
            return true;
        }
        if ( lead_byte >= 0xE1 && lead_byte <= 0xEC ) {
            pvt_advance(context, 1);
            // expect next 2 bytes in 0x80-BF
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)*context->current_ptr;
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)*context->current_ptr;
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            //happy path
            sb_append_char(sb, stream_bytes[0]);
            sb_append_char(sb, stream_bytes[1]);
            sb_append_char(sb, stream_bytes[2]);
            return true;
        }
        if (lead_byte == 0xED) {
            pvt_advance(context, 1);
            //expect next byte in 0x80-9F, then x80-BF
            // if second byte in 0xA0 to 0xBF, encoding a surrogate : forbidden
            current_byte = (uint8_t)*context->current_ptr;
            if ( current_byte >= 0xA0 && current_byte <= 0xAF) {
                snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE,
            "'0x%.2X' is an invalid UTF-8 continuation byte and reserved for high surrogates.", current_byte);
                context->parse_end = context->current_index;
                pvt_record_error(context, error, JSON_ERR_RESERVED_FOR_HIGH_SURROGATE, error_msg_buffer);
                return false;
            }
            if ( current_byte >= 0xB0 && current_byte <= 0xBF) {
                snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE,
            "'0x%.2X' is an invalid UTF-8 continuation byte and reserved for low surrogates.", current_byte);
                context->parse_end = context->current_index;
                pvt_record_error(context, error, JSON_ERR_RESERVED_FOR_LOW_SURROGATE, error_msg_buffer);
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
            current_byte = (uint8_t)*context->current_ptr;
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            //happy path
            sb_append_char(sb, stream_bytes[0]);
            sb_append_char(sb, stream_bytes[1]);
            sb_append_char(sb, stream_bytes[2]);
            return true;
        }
        if ( lead_byte == 0xEE || lead_byte == 0xEF) {
            pvt_advance(context, 1);
            // expect next 2 bytes in 0x80-BF
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)*context->current_ptr;
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)*context->current_ptr;
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            //happy path
            sb_append_char(sb, stream_bytes[0]);
            sb_append_char(sb, stream_bytes[1]);
            sb_append_char(sb, stream_bytes[2]);
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
            current_byte = (uint8_t)*context->current_ptr;
            if ( current_byte >= 0x80 && current_byte <= 0x8F) {
                snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE,
            "'0x%.2X' is an invalid UTF-8 continuation byte and overlong sequence.", current_byte);
                context->parse_end = context->current_index;
                pvt_record_error(context, error, JSON_ERR_OVERLONG_SEQUENCE, error_msg_buffer);
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
            current_byte = (uint8_t)*context->current_ptr;
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)*context->current_ptr;
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            //happy path
            sb_append_char(sb, stream_bytes[0]);
            sb_append_char(sb, stream_bytes[1]);
            sb_append_char(sb, stream_bytes[2]);
            sb_append_char(sb, stream_bytes[3]);

            return true;
        }

        if ( lead_byte >= 0xF1 && lead_byte <= 0xF3 ) {
            pvt_advance(context, 1);
            //expect next 3 bytes in x80-BF
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)*context->current_ptr;
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)*context->current_ptr;
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)*context->current_ptr;
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            //happy path
            sb_append_char(sb, stream_bytes[0]);
            sb_append_char(sb, stream_bytes[1]);
            sb_append_char(sb, stream_bytes[2]);
            sb_append_char(sb, stream_bytes[3]);

            return true;
        }

        if (lead_byte == 0xF4) {
            pvt_advance(context, 1);
            // expect next byte in 0x80-8F, then next two in x80-BF
            //  if second byte > 0x90, this exceeds legal Unicode limit
            current_byte = (uint8_t)*context->current_ptr;
            if ( current_byte >= 0x90 ) {
                snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE,
            "'0x%.2X' is an invalid UTF-8 continuation byte and out of range.", current_byte);
                context->parse_end = context->current_index;
                pvt_record_error(context, error, JSON_ERR_CODEPOINT_OUT_OF_RANGE, error_msg_buffer);
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
            current_byte = (uint8_t)*context->current_ptr;
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)*context->current_ptr;
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            //happy path
            sb_append_char(sb, stream_bytes[0]);
            sb_append_char(sb, stream_bytes[1]);
            sb_append_char(sb, stream_bytes[2]);
            sb_append_char(sb, stream_bytes[3]);

            return true;
        }
    }

    // C0-C1 is invalid.
    if (lead_byte == 0xC0 || lead_byte == 0xC1 ) {
        snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE,
    "'0x%.2X' is an invalid UTF-8 start byte and overlong sequence.", lead_byte);
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_OVERLONG_SEQUENCE, error_msg_buffer);
        return false;
    }

    snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE,
"'0x%.2X' is an invalid UTF-8 start byte.", lead_byte);
    context->parse_end = context->current_index;
    pvt_record_error(context, error, JSON_ERR_INVALID_UTF8_START_BYTE, error_msg_buffer);
    return false;
}

// assumes *context->current_ptr == 'u' or 'U' and the previous character was a backslash '\'
static StringBuilder * pvt_parse_unicode_escape( JsonContext *context, JsonError *error, Arena *arena, StringBuilder *sb_out ) {
    if (*context->current_ptr == 'U') {
        pvt_advance(context, 1);
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
        snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE, "Expected high surrogate to precede low surrogate '\\u%4X', but none found.", cp1);
        context->parse_end = context->current_index;
        pvt_record_error(context, error, JSON_ERR_NO_PRECEDING_HIGH_SURROGATE, error_msg_buffer);
        return nullptr;
    }
    uint32_t cp = cp1;

    if (cp1 >= 0xD800 && cp1 <= 0xDBFF ) {
        // High surrogate, expect the low surrogate to follow.
        // RFC 8259: To escape an extended character (>U+FFFF), it must be
        // represented as a 12-character sequence encoding the UTF-16 surrogate pair.
        // Non-standard escapes like \UXXXXXXXX are not supported.
        const char *current_ptr = context->current_ptr;

        // Check for enough remaining characters safely
        if (current_ptr[0] != '\\' || current_ptr[1] != 'u') {
            // no following low surrogate.
            snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE, "Expected low surrogate escape \\uXXXX to follow high surrogate '\\u%4X'.", cp1);
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_NO_FOLLOWING_LOW_SURROGATE, error_msg_buffer);
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
            snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE,
                "Expected low surrogate to follow '\\u%4X', but found '\\u%4X' instead.", cp1, cp2);
            context->parse_end = context->current_index;
            pvt_record_error(context, error, JSON_ERR_NO_FOLLOWING_LOW_SURROGATE, error_msg_buffer);
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

// todo we'll need to pass an Error record back with detailed error information. For now we return nullptr if error

constexpr char QUOTE           = 0x22;  // "
constexpr char REVERSE_SOLIDUS = 0x5c;  // \  backslash

static JsonValue * pvt_parse_string(JsonContext *context, JsonError *error, Arena *arena ) {
    StringBuilder sb;
    sb_init(&sb, 16, "");

    // we examine chars in the stream until we find:
    // 1. a closing quote, which is a quote not preceded by the backlash (reverse solidus)
    // 2. EOF, AKA null terminator, \x00
    JsonValue *value = nullptr;
    pvt_advance(context, 1); // Skip the opening quote
    unsigned char current_byte = (unsigned char )*context->current_ptr;
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
            current_byte = (unsigned char )*context->current_ptr;

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
                    sb_append_char(&sb, current_byte);
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
                    snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE, "Invalid escape sequence: \\%c", current_byte);
                    pvt_record_error(context, error, JSON_ERR_INVALID_ESCAPE_SEQUENCE, error_msg_buffer);
                    sb_destroy(&sb);
                    return nullptr;
            }

        } else if ( current_byte <= 0x1F) {
            // RFC 8259: Control characters U+0000 through U+001F MUST be escaped.
            // This means the literal bytes cannot appear here.
            snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE, "Unexpected unescaped control character: 0x%.2X\n", current_byte);
            pvt_record_error(context, error, JSON_ERR_UNESCAPED_CONTROL_CHAR, error_msg_buffer);
            sb_destroy(&sb);
            return nullptr;
        } else {
            // Here we validate a string of UTF-8 characters
            if (! pvt_validate_utf8(context, error, &sb)) return nullptr;
        }
        current_byte = (unsigned char)*context->current_ptr;
    }

    context->parse_end = context->current_index;
    pvt_record_error(context, error, JSON_ERR_UNTERMINATED_STRING, "No closing quote found.");
    sb_destroy(&sb);
    return nullptr;  // no closing quote found
}

// RSL: "Regex Start of Line"
#define RSL      "^"
#define WS       "[\x20\x09\x0A\x0D]"
#define WS_star  "[\x20\x09\x0A\x0D]*"
static const char * const REGEX_NUMBER_STR = RSL "(-?(0|([1-9][0-9]*))(\\.[0-9]+)?([eE][-+]?[0-9]+)?)" WS_star;
static regex_t REGEX_NUMBER_PATTERN;
constexpr int MATCH_FOUND = 0;

static JsonValue * pvt_parse_number(JsonContext *context, JsonError *error, Arena *arena ) {
    JsonValue *value = nullptr;

    constexpr size_t max_groups = 4;
    // The first element (0) is the entire match, subsequent elements are capture groups
    regmatch_t pmatch[max_groups] = {}; // Assuming max_groups - 1 capture groups + full match
    const int result = regexec( &REGEX_NUMBER_PATTERN, context->current_ptr, max_groups, pmatch, 0);
    int match_len =  (int)pmatch[0].rm_eo;
    if (result != MATCH_FOUND || match_len < 0) {
        snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE, "expected number, got: %.*s", 100 ,context->current_ptr);
        pvt_record_error(context, error, JSON_ERR_INVALID_NUMBER_FORMAT, error_msg_buffer);
        return nullptr;
    }
    auto start = pmatch[1].rm_so;
    auto end = pmatch[1].rm_eo;
    bool is_double = false;
    for (regoff_t i = start; i < end; ++i) {
        char c = context->current_ptr[i];
        if (c == '.' || c == 'e' || c == 'E') {
            is_double = true;
            break;
        }
    }

    value = arena_alloc(arena, sizeof(JsonValue) );
    value->type = JSON_NUMBER;

    errno = 0; // Reset errno before the calls
    if (is_double) {
        value->type = JSON_FLOAT;
        value->u.n_double = strtod(context->current_ptr, nullptr);
        // Note: If strtod overflows, u.n_double will be +/- Infinity (HUGE_VAL).
    } else {
        value->type = JSON_INT;
        long val = strtol(context->current_ptr, nullptr, 10);
        if (errno == ERANGE) {
            // Promotion: If too big for long, use double to preserve magnitude (even if it becomes Infinity)
            value->type = JSON_FLOAT;
            value->u.n_double = strtod(context->current_ptr, nullptr);
        } else {
            value->u.n_long = val;
        }
    }
    pvt_advance(context, match_len);
    context->parse_end = context->current_index;
    return value;
}

static JsonValue *  pvt_parse_literal_impl(  JsonContext *context,
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
        pvt_record_error(context, error, err_type, error_msg_buffer);
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

static JsonValue *  pvt_parse_true(JsonContext *context, JsonError *error ) {
    return pvt_parse_literal_impl(context, error, &JSON_TRUE_VALUE, JSON_KEYWORD_TRUE );
}

static JsonValue *  pvt_parse_false(JsonContext *context, JsonError *error ) {
    return pvt_parse_literal_impl(context, error, &JSON_FALSE_VALUE, JSON_KEYWORD_FALSE );
}

static JsonValue *  pvt_parse_null(JsonContext *context, JsonError *error ) {
    return pvt_parse_literal_impl(context, error, &JSON_NULL_VALUE, JSON_KEYWORD_NULL );
}

static JsonValue *pvt_parse_value(JsonContext *context, JsonError *error, Arena *arena ) {
    pvt_skip_whitespace(context);
    context->parse_start = context->current_index;
    JsonValue *value = nullptr;
    switch (*context->current_ptr) {
        case '{': /* Handle object */
            value = pvt_parse_object(context, error, arena);
            break;
        case '[': /* Handle array */
            value = pvt_parse_array(context, error, arena);
            break;
        case '"': /* Handle string */
            value = pvt_parse_string(context, error, arena);
            break;
        case '-': case '0': case '1': case '2': case '3':
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
                context->parse_end = context->current_index;
                snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE, "unexpected character:'%c'", *context->current_ptr);
                pvt_record_error(context, error, JSON_ERR_UNEXPECTED_TEXT, error_msg_buffer);
                return nullptr;
            }
            break;

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

static bool pvt_is_rejected_due_to_bom(JsonContext *context, const char *json_text, JsonError *error) {
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
        if (jsonp_is_flag_set(JSON_CONFIG_FAIL_ON_INITIAL_BOM)) {
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

static JsonValue * pvt_jsonp_parse_impl(JsonContext *context, const char *json_text, JsonError *error, Arena *arena) {
    if (!json_text) {
        *error = (JsonError){ .json=json_text, .message = "null json text", .err_type = JSON_ERR_NULL_TEXT};
        return nullptr;
    }
    if (json_text[0] == '\0') {
        *error = (JsonError){.json=json_text, .message = "empty json text", .err_type = JSON_ERR_EMPTY_TEXT};
        return nullptr;
    }

    if (pvt_is_rejected_due_to_bom(context, json_text, error)) return nullptr;

    pvt_skip_whitespace(context);
    if (json_text[0] == '\0') {
        *error = (JsonError){.json=json_text, .message = "empty json text", .err_type = JSON_ERR_EMPTY_TEXT,
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

    if (*context->current_ptr != '\0') {
        //we parsed the root value, but there is still text remaining in the JSON text, which is an error
        pvt_record_error(context, error, JSON_ERR_UNEXPECTED_TEXT, "unexpected json text after parsing value");
        return nullptr;
    }
    return value;
}


JsonValue *jsonp_parse(const char *json_text, JsonError *error, Arena *arena) {
    JsonContext context = {.current_ptr = json_text, .json=json_text};
    JsonValue *value = pvt_jsonp_parse_impl(&context, json_text, error, arena);
    return value;
}

JsonValue *jsonp_parse_ex(const char *json_text, JsonError *error, Arena *arena, const uint32_t buffer_size) {
    JsonContext context = {.current_ptr = json_text, .json=json_text};
    JsonValue *value = pvt_jsonp_parse_impl(&context, json_text, error, arena);
    if (!value) return nullptr;

    if (context.current_index < buffer_size) {
        snprintf(error_msg_buffer, ERROR_MSG_BUFFER_SIZE,
    "JSON text was successfully parsed, but characters remain in the buffer."
    " This can happen when the text is followed by embedded NUL characters.\nbuffer size=%u, bytes parsed=%u",
    buffer_size, context.current_index );
        context.parse_end = context.current_index;
        pvt_record_error(&context, error, JSON_ERR_UNEXPECTED_EOF, error_msg_buffer);
        return nullptr;

    }
    return value;
}


constexpr int REGEX_COMPILE_SUCCESS = 0;
static bool is_initialized = false;

Error jsonp_init() {
    int reti = regcomp(&REGEX_NUMBER_PATTERN, REGEX_NUMBER_STR, REG_EXTENDED);
    if ( reti != REGEX_COMPILE_SUCCESS) {
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
        case JSON_INT:
            printf("%ld", value->u.n_long);
            break;
        case JSON_FLOAT:
            printf("%g", value->u.n_double);
            break;
        case JSON_STRING:
            printf("'%s'\n", value->u.string);
            // todo temp
            uint32_t len = strlen(value->u.string);
            printf("raw bytes: %u ", len);
            for (uint32_t i = 0; i < len; ++i) {
                printf(" %.1X", (uint8_t)value->u.string[i]);
            }
            break;
        case JSON_ARRAY:
            json_array_str(value);
            break;
        case JSON_OBJECT:
            pvt_json_object_str(value);
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
    if (init_err.err) {
        err_print(init_err);
        jsonp_destroy();
        return;
    }
    Arena arena = {};
    ArenaErrResult aer = arena_create_arena( &arena, ONE_MIBIBYTE * 100);
    if ( aer.err ) {
        printf("arena_create_arena failed with %d, %s\n", aer.reported_err, aer.msg);
    }
    JsonError err = {.json = str};
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

void test_multi_byte_char_strings() {
    // one byte
    test_parse_str("\"\x41\"");     // A

    // two bytes
    test_parse_str("\"\xC1\x80\""); // invalid
    test_parse_str("\"\xC2\x80\""); // ''

    // three bytes
    test_parse_str("\"\xE0\xA0\x80\""); // 'ࠀ'
    test_parse_str("\"\xE1\x95\xBB\""); // 'ᕻ'
    test_parse_str("\"\xED\x95\xBB\""); // '핻'
    test_parse_str("\"\xEE\x95\xBB\""); // ''
    test_parse_str("\"\xEF\xBF\xBF\""); // '￿'

    // four bytes
    test_parse_str("\"\xF0\x90\x80\x80\""); // '𐀀'
    test_parse_str("\"\xF0\x9F\x98\x80\""); // '😀'
    test_parse_str("\"\xF0\xBF\xBF\xBF\""); // '𿿿'

    test_parse_str("\"\xF1\x80\x80\x80\""); // '𿿿'
    test_parse_str("\"\xF3\xBF\xBF\xBF\""); // '󿿿'

    test_parse_str("\"\xF4\x80\x80\x80\""); // '󿿿'
    test_parse_str("\"\xF4\x8F\xBF\xBF\""); // '󿿿'

    test_parse_str("\"\xF4\x90\x80\x80\""); // '󿿿'


}

void test_parse_unicode_escapes() {
    // test_parse_str("\"\\uCAFE \\uBABE\"");
    // test_parse_str("\"\\uCAFE\\uBABE\"");

    // test_parse_str("\"\\uD801\\uDC01\"");   // valid high- / low-surrogate pair=
    //
    // test_parse_str("\"\\uDC01\"");      //low surrogate without preceding high surrogate
    // test_parse_str("\"\\uD801\"");      //high surrogate without following low surrogate
    //
    // test_parse_str("\"\\uDC01\\uDC02\"");      //two low surrogates in a row
    // test_parse_str("\"\\uD801\\uD802\"");      //two high surrogates in a row


    test_parse_str("\"\\u0041\""); // A
    test_parse_str("\"\\u0080\"");  // ''

    test_parse_str("\"\\u0800\"");  // 'ࠀ'

    test_parse_str("\"\\uD834\\uDD1E\"");

    test_parse_str("\"😀  \\uD83D\\uDE00\"");

    test_parse_str("\"😀  \\U01F600\"");  // Our custom enhancement! Allows full unicode codepoint without surrogates


    // the code that renders glyphs combines them into one glyph!!
    test_parse_str("\" combining character: C with tail: \\u0043\\u0327\"");

    // Test: Combining character vs Precomposed
    // \u00E9 is 'é' (1 codepoint)
    // e\u0301 is 'e' + '´' (2 codepoints)
    printf("\n--- Combining Character Test ---\n");
    test_parse_str("\"\\u00E9\"");
    test_parse_str("\"e\\u0301\"");
    printf("Note: Both should look identical in the terminal, but 'raw bytes' count will differ.\n");

    // test_parse_str("\"\\uABCDAPPLE\"");
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
    // test_parse_str("0");
    // test_parse_str("1");
    // test_parse_str("1.1");
    // test_parse_str("-3.3");
    // test_parse_str("-3.3e24");
    // test_parse_str("5.3e-24");
    // test_parse_str("5.67 moo");
    // test_parse_str("[100000000000000000000]");
    test_parse_str("[123123e100000]");
    test_parse_str("[-123123e100000]");
    test_parse_str("[0.4e00669999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999969999999006]");
    test_parse_str("[0.4e-00669999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999969999999006]");
    test_parse_str("[-0.4e-00669999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999969999999006]");

    test_parse_str("[-123123123123123123123123123123]");
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
    test_parse_str("[[1 ], [2] ] ");
    test_parse_str("[[1,2] ] ");
    test_parse_str("[[1,2],[3,4]] ");
    test_parse_str("[ [1,2],[3,4]] ");
    test_parse_str("[ [1,2], [3,4]] ");
    test_parse_str("[ [1,2], [3,4] ] ");

    test_parse_str("[ [1,2,3], [4,5,6], [true,false,null] ] ");

}

void test_parse_objects(void ) {
    test_parse_str("{}");   // empty object is fine by the spec

    test_parse_str("{ \"foo\": null }");

    test_parse_str("{ \"one\": \"one\", \"two\" : 2, \"three\" : 3.33 }   ");

    test_parse_str("{ \"one\": \"one\", \"two\" : 2, \"three\" : 3.33, \"null\" : null, \"true\":true, \"false\":false }");

    test_parse_str(" [{ \"name\": \"jelly bowl\", \"ff\": 5}, {\"name\": \"werewolf\", \"ff\": 10 }]");
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
    // test_parse_str("[\"日ш�\"]");

    // a really big int.
    test_parse_str("[100000000000000000000]");  // parses as [ 1e+20 ]


    // + overflow
    test_parse_str("[123123e100000]"); // [ inf ]

    // - overflow
    test_parse_str("[-123123e100000]");  // [ -inf ]

    // huge exp- parses as [ inf ]
    test_parse_str("[0.4e00669999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999969999999006]");

    // too big negative int
    test_parse_str("[-123123123123123123123123123123]");  // [ -1.23123e+29 ]

    // number_double_huge_neg_exp.json
    test_parse_str("[123.456e-789]");  // [ 0 ]

    //number_pos_double_huge_exp.json
    test_parse_str("[1.5e+9999]"); // [ inf ]

    //number_real_underflow.json
    test_parse_str("[123e-10000000]");  // [ 0 ]

    // number_very_big_negative_int.json  - parses a [ -2.37462e+47 ][ -2.37462e+47 ]
    test_parse_str("[-237462374673276894279832749832423479823246327846]");

    // number_neg_int_huge_exp.json
    test_parse_str("[-1e+9999]");  // [ -inf ]
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


}
#endif
