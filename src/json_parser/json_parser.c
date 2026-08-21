//  json_parser.c
// Created by Rob Ross on 7/2/26.
//

// version: JSONP v0.1.1


#include "roblib/json_parser.h"
#include <ctype.h>

#include <locale.h>
#include <sys/stat.h>
#include <unistd.h> // For access() or stat() on POSIX
#include <errno.h>
#include <stdatomic.h>

#include <curl/curl.h>

#include "roblib/string_builder.h"
#include "roblib/char_ring_buffer.h"
#include "roblib/unicode_tools.h"
#include "roblib/base.h"
#include "roblib/constants.h"

/*
 *  todo (rob) tasks:
 *  1. pretty printer
 *  2. file writer to save JSON to FILE*
 *  3. API for parsing a FILE*?
 *  4. implement all current config flag options in code
 *      - JSON_CONFIG_ALLOW_HEX_x_ESCAPE
 *          two issues that complicate this.
 *              1. Every \xXX is 4 characters, but this results in a single byte in the string
 *              2. We need to feed a converted \xXX value to the utf-8 verifier as we parse.
 *                  Mixing escapes and bytes complicates things. If \xXX is a leading byte and the next two
 *                  bytes in the stream are valid continuation bytes, the multi-byte char is valid.
 *
 *  7. add ability to change depth on context
 *  8. add ability to change whitespace on context
 *  9. documentation
 *  10. testing.
 *  11. add warnings if int promoted to float, or float over/underflows
 */


static constexpr char NUL = '\0';
static constexpr size_t LOOK_AHEAD_BUF_SIZE = 40;
char const * const    JSON_WHITESPACE_CHARS_DEFAULT = " \t\n\r";


// -----------------------------------------------------------------
//      BOM CONSTANTS
// -----------------------------------------------------------------

static constexpr uint8_t BOM_UTF8[]     = { 0xEF, 0xBB, 0xBF };
static constexpr uint8_t BOM_UTF16_BE[] = { 0xFE, 0xFF };
static constexpr uint8_t BOM_UTF16_LE[] = { 0xFF, 0xFE };
static constexpr uint8_t BOM_UTF32_BE[] = { 0x00, 0x00, 0xFE, 0xFF };
static constexpr uint8_t BOM_UTF32_LE[] = { 0xFF, 0xFE, 0x00, 0x00 };



// -----------------------------------------------------------------
//      INPUT DYNAMIC FUNCTIONS
// -----------------------------------------------------------------

typedef long (*read_fn)( void *context, size_t max_bytes );
typedef int  (*current_char_fn)( void *context );
typedef int  (*peek_next_char_fn)( void *context );
typedef int  (*peek_lookahead_chars_fn)( void *context, uint32_t lookahead );
typedef uint32_t (*advance_n_bytes_fn)( void *context, uint32_t num_bytes );
typedef void (*sprint_n_lookahead_chars_fn)(void *context, uint32_t n_chars, char buffer[ static n_chars + 1 ] );


// CHAR_RING_BUFFER(CharRingBuffer, 40)


typedef struct {
    void                        *input_context;  // StringSourceInputContext, or FileSourceInputContext
    CharRingBuffer              *look_behind_buffer; // holds most recent 40 bytes of scanned input

    uint32_t       current_byte_index; // the index of the byte-char the lexer is scanning
    uint32_t       line;
    uint32_t       column;
    uint32_t       parse_start;
    uint32_t       parse_end;

    read_fn                     read;
    current_char_fn             current_char;
    peek_next_char_fn           peek_next_char;
    peek_lookahead_chars_fn     peek_lookahead_chars;
    advance_n_bytes_fn          advance_n_bytes;
    sprint_n_lookahead_chars_fn sprint_n_lookahead_chars;

} Input;

// ReSharper disable once CppUseInternalLinkage
typedef struct json_context_s {
    Input          *input;

    uint32_t       depth_current;
    uint32_t       depth_max;
    jp_bitset_t    config_flags;
    char           decimal_separator; // used when converting float values
    char           whitespace_chars[16];
    bool           ws_table[256];
    char           error_msg[ERROR_MSG_BUFFER_SIZE + 1];
} JsonContext;



// -----------------------------------------------------------------
//      File
// -----------------------------------------------------------------


/**
 * CharRingBuffer40 is declared and defined by the CHAR_RING_BUFFER(CharRingBuffer, 40) macro above. <P>
 * The instance pointed to by `look_behind_buffer` must be the same instance as the CharRingBuffer40 in the
 * enclosing Input object's CharRingBuffer40 `look_behind_buffer` member.<P>
 * Currently, the lifecycles of both Input and the InputContext (here `FileSourceInputContext`) are scoped to
 * the same parse API function call, and each Input gets a unique InputContext.
 * Neither are intended to be exported. <P>
 */
typedef struct {
    // Parent/enclosing Input object. Creates a cycle, but both are stack objects and created in same block scope
    // and go out of scope at the same time.
    Input               *input;

    const char          *json_file_full_path;
    const char          *json_filename;
    FILE                *file_ptr;
    // scanner_buffer is private to this FileSourceInputContext instance
    CharRingBuffer      *scanner_buffer;     // next 40 bytes from file

    size_t              length_bytes;       // 0 if unknown (streams/pipes)
} FileSourceInputContext;


static long file_read( void *context, size_t requested_bytes)
{
    FileSourceInputContext *src = context;
    Input *input = src->input;
    CharRingBuffer *scan_buf = src->scanner_buffer;

    const size_t capacity = scan_buf->capacity;
    if (src->length_bytes > 0) {
        // if we know the length of the stream, we'll try to read the max capacity bytes.
        // if we don't know the length of the stream, we'll honor requested_bytes
        requested_bytes = capacity;
    }

    uint32_t wanted_bytes = requested_bytes > ( capacity - scan_buf->length) ? capacity - scan_buf->length : requested_bytes;

    if (! wanted_bytes ) return 0;  // nothing to read

    FILE *fp = (FILE*)src->file_ptr;

    // If length is unknown (0), we just try to read. Otherwise, respect the limit.
    size_t remaining = (src->length_bytes == 0) ? wanted_bytes : (src->length_bytes - input->current_byte_index);

    if (src->length_bytes > 0 && remaining == 0) return EOF;

    if (remaining > wanted_bytes) remaining = wanted_bytes;
    char buf[41] = {};
    size_t bytes_read = fread( buf, 1, remaining, fp);
    // printf("bytes_read=%zu,", bytes_read);

    if (bytes_read < 0) {
        if (ferror(fp)) {
            perror("Error reading file");
        }
        return EOF;
    }
    crb_add_str_to_buffer_strict_CharRingBuffer(scan_buf, bytes_read, buf);
    // printf("src->position=%u\n", input->current_byte_index);
    return (long)bytes_read;
}

// try to ensure the CharRingBuffer has num_bytes elements, read from file if needed
static long file_ensure_char_ring_buffer_length(FileSourceInputContext *src, size_t num_bytes) {
    Input *input = src->input;
    CharRingBuffer *scan_buf = src->scanner_buffer;
    if ( scan_buf->length >= num_bytes ) {
        return (long)num_bytes;  // buffer is already at desired length
    }
    // If length is 0 (unknown), assume there is more data until fread tells us otherwise
    size_t remaining = (src->length_bytes == 0) ? num_bytes : (src->length_bytes - input->current_byte_index);
    num_bytes = MIN(num_bytes, remaining);

    // read more characters into the scanner buffer.
    return input->read(src, num_bytes);  // try to fill as much of the buffer as we can
}

static int file_current_char(void *context) {
    FileSourceInputContext *src = context;
    Input *input = src->input;

    if ( src->length_bytes > 0 && input->current_byte_index == src->length_bytes ) return EOF;

    CharRingBuffer *scan_buf = src->scanner_buffer;
    long result = file_ensure_char_ring_buffer_length(src, 1);
    if (result < 0) {
        // error
        printf("file_ensure_char_ring_buffer_capacity() returned %ld in file_current_char", result);
        return EOF;
    }
    return crb_peek_char_CharRingBuffer(scan_buf, 0);
}

static int file_peek_next_char(void *context) {
    FileSourceInputContext *src = context;
    Input *input = src->input;
    if ( src->length_bytes > 0 && input->current_byte_index == src->length_bytes ) return EOF;

    CharRingBuffer *scanner_buffer = src->scanner_buffer;
    long result = file_ensure_char_ring_buffer_length(src, 2);
    if (result < 2) {
        // error
        printf("file_ensure_char_ring_buffer_capacity() returned %ld in file_peek_next_char()", result);
        return EOF;
    }
    return crb_peek_char_CharRingBuffer(scanner_buffer, 1);
}

// we only intend to support max lookahead of:
// 6 for \U0XXXXX, 4 for \uXXXX, 3 for BOM, 2 for \t, etc.
static int file_peek_lookahead_chars(void *context, uint32_t lookahead ) {
    FileSourceInputContext *src = context;
    Input *input = src->input;

    if ( src->length_bytes > 0 && input->current_byte_index + lookahead >= src->length_bytes )
        return EOF;

    // return src->json_text[src->position + lookahead];
    CharRingBuffer *scanner_buffer = src->scanner_buffer;
    long result = file_ensure_char_ring_buffer_length(src, lookahead + 1);
    if (result < lookahead) {
        // error
        printf("file_ensure_char_ring_buffer_capacity() returned %ld in file_peek_lookahead_chars()", result);
        return EOF;
    }
    return crb_peek_char_CharRingBuffer(scanner_buffer, lookahead);
}

static uint32_t file_advance_n_bytes( void *context, uint32_t num_bytes) {
    FileSourceInputContext *src = context;
    Input *input = src->input;
    CharRingBuffer *look_behind_buffer = input->look_behind_buffer;
    CharRingBuffer *scanner_buffer = src->scanner_buffer;

    // add chars to look-behind-buffer
    const size_t capacity = look_behind_buffer->capacity;

    long result = file_ensure_char_ring_buffer_length(src, num_bytes);
    if (result < num_bytes) {
        printf("file_ensure_char_ring_buffer_capacity() returned %ld in file_advance_n_bytes()", result);
        return EOF;
    }

    if (num_bytes == 1 ) {
        int cur_char = crb_get_next_char_CharRingBuffer(scanner_buffer);
        // todo error checking
        crb_add_char_to_buffer_CharRingBuffer(look_behind_buffer,  (char)cur_char);
    } else if (num_bytes > 1) {
        size_t num_to_add = num_bytes;
        if (num_bytes > capacity) {
            // we'll only add the last `capacity` bytes.
            num_to_add = MIN(capacity, 128);
        }
        char cb[128] = {};
        // write the contents of scanner_buffer into the `cb` array
        crb_sprint_buffer_CharRingBuffer(scanner_buffer, cb);
        // advance the scanner_buffer by the requested, clamped amount
        crb_advance_buffer_CharRingBuffer(scanner_buffer, num_to_add);
        // copy the chars we advanced past into the look-behind_buffer
        crb_add_str_to_buffer_CharRingBuffer(look_behind_buffer, num_to_add, cb);
    }

    if (src->length_bytes > 0 && input->current_byte_index >= src->length_bytes - num_bytes  ) {
        input->current_byte_index = src->length_bytes;
        return 0;
    }
    //todo this isn't correct. num_bytes needs to be clamped to bytes remaining
    input->current_byte_index += num_bytes;
    return num_bytes;
}

// copies up to `n_chars` chars forward from the current position of the JSON text.
static void file_sprint_n_lookahead_chars(void *context, const uint32_t n_chars, char buffer[ static n_chars + 1 ] ) {
    FileSourceInputContext *src = context;
    Input *input = src->input;
    if ( src->length_bytes > 0 && input->current_byte_index == src->length_bytes) return;

    if ( src->length_bytes > 0 ) {
        long result = file_ensure_char_ring_buffer_length(src, n_chars);
        if (result < 0) {
            // error
            printf("file_ensure_char_ring_buffer_capacity() returned %ld in file_sprint_n_lookahead_chars", result);
        }
    }

    // printf("file_sprint_n_lookahead_chars: length_bytes:%zd, filename:%s\n", src->length_bytes, src->json_filename);

    CharRingBuffer *scanner_buffer = src->scanner_buffer;

    crb_sprint_buffer_CharRingBuffer(scanner_buffer, buffer );
}

// -----------------------------------------------------------------
//      String
// -----------------------------------------------------------------

/**
 * CharRingBuffer40 is declared and defined by the CHAR_RING_BUFFER(CharRingBuffer, 40) macro above. <P>
 * The instance pointed to by `look_behind_buffer` must be the same instance as the CharRingBuffer40 in the
 * enclosing Input object's CharRingBuffer40 `look_behind_buffer` member.<P>
 * Currently, the lifecycles of both Input and the InputContext (here `StringSourceInputContext`) are scoped to
 * the same parse API function call, and each Input gets a unique InputContext.
 * Neither are intended to be exported. <P>
 */
typedef struct {
    // Parent/enclosing Input object. Creates a cycle, but both are stack objects and created in the same block scope
    // and go out of scope at the same time.
    Input               *input;
    const char          *json_text;             // full original JSON text string
    size_t              length_bytes;           // total length of the json_text C-string in bytes
} StringSourceInputContext;

static long string_read( void *context, size_t max_bytes) {
    // this is basically a no-op since the full json_text is already in memory at src->json_text

//     StringSourceInputContext *src = context;
//     Input *input = src->input;
//     if (input->current_byte_index == src->length_bytes) return EOF;
//     size_t return_bytes = max_bytes > src->length_bytes ? src->length_bytes : max_bytes;
//     return (long)return_bytes;
    return 0;
}

static int string_current_char(void *context) {
    StringSourceInputContext *src = context;
    Input *input = src->input;
    if (input->current_byte_index == src->length_bytes) return (unsigned char)src->json_text[src->length_bytes];
    return (unsigned char)src->json_text[input->current_byte_index];
}

static int string_peek_next_char(void *context) {
    StringSourceInputContext *src = context;
    Input *input = src->input;
    if (input->current_byte_index + 1 >= src->length_bytes) return EOF;

    return (unsigned char)src->json_text[input->current_byte_index + 1];
}

// we only intend to support max lookahead of 6
static int string_peek_lookahead_chars( void *context, uint32_t lookahead ) {
    StringSourceInputContext *src = context;
    Input *input = src->input;
    if (input->current_byte_index + lookahead >= src->length_bytes) return EOF;
    return (unsigned char)src->json_text[input->current_byte_index + lookahead];
}

static uint32_t string_advance_n_bytes( void *context, const uint32_t num_bytes) {
    StringSourceInputContext *src = context;
    Input *input = src->input;
    CharRingBuffer *look_behind_buffer = input->look_behind_buffer;

    // add chars to look-behind-buffer
    const size_t capacity = look_behind_buffer->capacity;
    char const * start_ptr = src->json_text + input->current_byte_index;

    if (num_bytes == 1 ) {
        crb_add_char_to_buffer_CharRingBuffer(look_behind_buffer,  *start_ptr);
    } else if (num_bytes > 1) {
        size_t num_to_add = num_bytes;
        if (num_bytes > capacity) {
            // we'll only add the last `capacity` bytes.
            num_to_add = MIN(capacity, 128);
            start_ptr += num_bytes - num_to_add;
        }
        char cb[128] = {};
        int n = snprintf(cb, num_to_add + 1, "%s", start_ptr);
        if (!n) {
            fprintf(stderr, "snprintf returned %d in string_advance_n_bytes", n);
        } else {
            crb_add_str_to_buffer_CharRingBuffer(look_behind_buffer, num_to_add, cb);
        }
    }

    if (input->current_byte_index + num_bytes  > src->length_bytes ) {
        input->current_byte_index  = src->length_bytes;
        return 0;
    }
    input->current_byte_index  += num_bytes;
    return num_bytes;
}

// copies up to `n_chars` chars forward from the current position of the JSON text.
static void string_sprint_n_lookahead_chars(void *context, const uint32_t n_chars, char buffer[ static n_chars + 1 ] ) {
    StringSourceInputContext *src = context;
    Input *input = src->input;

    char const *ptr = src->json_text + input->current_byte_index;
    size_t index = 0;
    while ( *ptr && index < n_chars ) {
        buffer[index++] = *ptr++;
    }
    buffer[n_chars] = NUL;
}







// -----------------------------------------------------------------
//      Forward Declarations
// -----------------------------------------------------------------
static JsonValue * pvt_parse_value(JsonContext *context, JsonParseError *error, AlokArena *arena );
static JsonValue * pvt_parse_string(const JsonContext *context, JsonParseError *error, AlokArena *arena );
static void pvt_init_context_whitespace_table(JsonContext *context);
static char pvt_current_char(JsonContext const *context);
static bool pvt_starts_with_bom(JsonContext const *context, uint32_t n_bytes, uint8_t const bom_bytes[static n_bytes]);
static bool pvt_is_rejected_due_to_bom(const JsonContext *context, JsonParseError *error);
static void pvt_advance(const JsonContext *context, uint32_t char_count);


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
    snprintf(context->whitespace_chars, MIN(sizeof(context->whitespace_chars), 8), "%s", whitespace_chars);
    pvt_init_context_whitespace_table(context);
}

static inline bool pvt_is_json_whitespace(const JsonContext *context, const unsigned char c) {
    return context->ws_table[c];
}

static void pvt_skip_whitespace(const JsonContext *context) {
    Input *input = context->input;
    char c;
    while ( c = pvt_current_char(context), pvt_is_json_whitespace(context, (const unsigned char)c ) ) {
        if ( c == '\n' || c == '\r') {
            input->line++;
            pvt_advance(context, 1);
            input->column = 0;
        } else {
            pvt_advance(context, 1);
        }
    }
}

// -----------------------------------------------------------------
//      NESTING DEPTH
// -----------------------------------------------------------------

static _Atomic(uint32_t) pvt_depth_max = JSON_DEPTH_MAX_DEFAULT;

void jsonp_set_context_max_depth(JsonContext *context, uint32_t max_depth){
    context->depth_max = max_depth;
}

uint32_t jsonp_get_context_max_depth(const JsonContext *context) {
    return context->depth_max;
}

// advance the parser state based on the current parse window
static void pvt_advance(const JsonContext *context, const uint32_t char_count) {;
    uint32_t bytes_advanced = context->input->advance_n_bytes(context->input->input_context, char_count);
    Input *input = context->input;
    input->column += bytes_advanced;
}

static char pvt_current_char(JsonContext const *context) {
    Input *input = context->input;
    return (char)input->current_char(input->input_context);
}

static char pvt_peek_next_char(JsonContext const *context) {
    Input *input = context->input;
    return (char)input->peek_next_char(input->input_context);
}

// Writes to the argument buffer.
// If `c` is a control character ( < 0x20 or > 0x7E) it will format `c` as a hex byte as 0xXX.
// Otherwise, formats `c` as a char.
static void pvt_format_error_message_char(
    const uint32_t buffer_size, char msg_buffer[static buffer_size], const char c ) {

    const unsigned char u_char  = (unsigned char)c;
    if ( u_char == 0x00 ) {
        snprintf(msg_buffer, buffer_size, "NUL");
    } else if ( u_char == 0xFF) {
        snprintf(msg_buffer, buffer_size, "EOF");
    } else if ( u_char < 0x20 || u_char > 0x7E) {
        // display non-ascii and control chars as a hex escape.
        snprintf(msg_buffer, buffer_size,
            "0x%.2X", u_char );
    } else {
        snprintf(msg_buffer, buffer_size,
            "'%c'", u_char );
    }
}

static void pvt_record_error(const JsonContext *context,
                                JsonParseError *error,
                                const JsonParseErrType err_type,
                                const char *msg) {

    // the most typical case is the parse ends at the current position when there is an error
    // exceptional situations may require this value to be changed by the caller on return from this method
    Input *input = context->input;
    input->parse_end = input->current_byte_index;

    if (error->message != msg ) {
        strncpy(error->message, msg, ERROR_MSG_BUFFER_SIZE);
    }

    crb_sprint_buffer_CharRingBuffer(context->input->look_behind_buffer, error->look_behind_buffer);
    input->sprint_n_lookahead_chars(input->input_context, 40, error->look_ahead_buffer);

    error->err_type = err_type;
    error->first_bad_char = input->current_byte_index;
    error->line   = input->line;
    error->column = input->column;
    error->parse_start = input->parse_start;
    error->parse_end = input->parse_end;
}

static void pvt_record_missing_comma_error(const JsonContext *context, JsonParseError *error) {
    int written = snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "expected comma, got ");
    if (written > 0) {
        pvt_format_error_message_char(ERROR_MSG_BUFFER_SIZE - written,
            error->message + written, pvt_current_char(context) );
    }
    pvt_record_error(context, error, JSON_ERR_MISSING_COMMA, error->message);
}


static bool pvt_max_depth_exceeded(const JsonContext *context, JsonParseError *error) {
    if (context->depth_current > context->depth_max) {
        snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
            "max nested depth of %u exceeded", context->depth_max);
        pvt_record_error(context, error, JSON_ERR_MAX_NESTED_DEPTH_EXCEEDED, error->message);
        return true;
    }
    return false;
}


// ReSharper disable once CppUseInternalLinkage
typedef struct json_object_entry_node_s { // NOLINT(modernize-use-anonymous-namespace)
    JsonObjectEntry *object_entry;
    struct json_object_entry_node_s *next;
} JsonObjectEntryNode;

static JsonObjectEntryNode * pvt_add_json_object_entry_node(JsonObjectEntryNode * first_node, JsonObjectEntry *object_entry, AlokArena *arena ) {
    if (!object_entry) return nullptr;
    JsonObjectEntryNode * new_node = (JsonObjectEntryNode*) alok_arena_alloc(arena, sizeof(JsonObjectEntryNode) );
    new_node->object_entry = object_entry;
    new_node->next = first_node;
    return new_node;
}

// todo can we pass `key` as a StringSlice?
JsonObjectEntry * jsonp_entry_for_key(const JsonValue *json_obj, char const * key) {
    for (uint32_t i = 0; i < json_obj->u.object.count; ++i) {
        StringSlice entry_key = json_obj->u.object.entries[i]->key;
        if (slice_equal(entry_key, slice_from_cstring(key)) == 0 ) {
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

static JsonObjectEntry * pvt_parse_one_entry(JsonContext *context, JsonParseError *error, AlokArena *arena) {
    pvt_skip_whitespace(context);
    Input *input = context->input;
    // We need to assign to parse_start here since we aren't calling pvt_parse_value(),
    // which is where we normally set this as we start to parse a JSON value

    input->parse_start = input->current_byte_index;
    if (pvt_current_char(context) != '"') {
        int written = snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "expected object key, got ");
        if (written > 0) {
            pvt_format_error_message_char(ERROR_MSG_BUFFER_SIZE - written,
                error->message + written, pvt_current_char(context) );
        }

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
    if (pvt_current_char(context) != ':' ) {
        pvt_record_error(context, error, JSON_ERR_MISSING_COLON, "expected name-separator ':'");
        return nullptr;
    }
    pvt_advance(context, 1);  // consume ':'
    pvt_skip_whitespace(context);
    if (pvt_current_char(context) == NUL) {
        // snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "unexpected EOF, expected object value");

        int written =  snprintf(error->message, ERROR_MSG_BUFFER_SIZE,"expected object value for key '%s', got ", SLICE_BUF(key->u.string, 128));
        if (written > 0) {
            pvt_format_error_message_char(ERROR_MSG_BUFFER_SIZE - written,
                error->message + written, pvt_current_char(context) );
        }

        pvt_record_error(context, error, JSON_ERR_MISSING_OBJECT_VALUE, error->message);
        return nullptr;
    }

    JsonValue *value = pvt_parse_value(context, error, arena);
    if (!value) {
        // error-out immediately
        return nullptr;
    }
    JsonObjectEntry *joe = (JsonObjectEntry*)alok_arena_alloc(arena, sizeof(JsonObjectEntry));
    if (!joe) {
        char const *const msg = "pvt_parse_one_entry(): arena alloc failed for JsonObjectEntry *joe\n";
        fprintf(stderr, msg);
        pvt_record_error(context, error, JSON_ERR_OUT_OF_MEMORY, msg);
        return nullptr;
    }

    joe->key = key->u.string;
    joe->value = value;
    return joe;
}

static JsonValue * pvt_parse_object(JsonContext *context, JsonParseError *error, AlokArena *arena ) {
    // we recursively parse members of this object until we see end-of-object '}' char or run out of text
    // todo refactor to use stack DS and iterative algorithm instead of recursion
    context->depth_current++;
    if ( pvt_max_depth_exceeded(context, error)) {
        context->depth_current--;
        return nullptr;  // error-out immediately
    }

    Input *input = context->input;
    JsonValue *object =  alok_arena_alloc(arena, sizeof(JsonValue) );

    size_t num_entries = 0;
    object->type = JSON_OBJECT;
    object->u.object.count   = num_entries;
    object->u.object.entries = nullptr;

    JsonObjectEntryNode *entries = nullptr;
    pvt_advance(context, 1);  // consume '{'
    pvt_skip_whitespace(context);

    //parse first entry, then parse (comma, element)s until '}' or EOF
    if ( pvt_current_char(context) && pvt_current_char(context) != '}') {
        JsonObjectEntry *joe = pvt_parse_one_entry(context, error, arena);
        if (!joe) {
            context->depth_current--;
            return nullptr;  // error report filled by previous call
        }
        entries = pvt_add_json_object_entry_node(entries, joe, arena);
        num_entries++;
        pvt_skip_whitespace(context);
    }


    while ( pvt_current_char(context) && pvt_current_char(context) != '}' ) {
        pvt_skip_whitespace(context);
        // expect ','
        if (pvt_current_char(context) != ',' ) {
            pvt_record_missing_comma_error(context, error);
            context->depth_current--;
            return nullptr;
        }
        // comma is expected delimiter between object entries
        uint32_t comma_index = input->current_byte_index; // save comma position for error reporting below
        pvt_advance(context, 1);  // consume ','
        pvt_skip_whitespace(context);

        if (pvt_current_char(context) == '}') {
            // we had a comma without another entry
            if (jsonp_is_context_config_flag_set(context, JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_OBJECTS)) {
                // this allowed
                break;
            }
            // this is an error
            input->parse_start = comma_index;
            pvt_record_error(context, error, JSON_ERR_TRAILING_COMMA_NOT_ALLOWED, "trailing comma not allowed in object entry list");
            // adjust error reporting so we indicate that the comma is the bad character
            error->first_bad_char = comma_index;
            error->column = comma_index;
            error->lab_offset = input->current_byte_index - comma_index;


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

    if (pvt_current_char(context) != '}' ) {
        pvt_record_error(context, error, JSON_ERR_UNTERMINATED_OBJECT, "missing closing brace '}'");
        context->depth_current--;
        return nullptr;
    }

    pvt_advance(context, 1);  // consume '}'
    JsonObjectEntry **entries_array = alok_arena_alloc(arena, sizeof(JsonObjectEntry) *  num_entries);
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

// ReSharper disable once CppUseInternalLinkage
typedef struct json_value_node_s {
    JsonValue *value;
    struct json_value_node_s *next;
} JsonValueNode;

// add a node to the linked list headed by first_node.
// adds nodes to the front of the linked list. If first_node is null, it will be the head node of a new linked list.
// Returns the new first_node.
static JsonValueNode * pvt_add_json_value_node(JsonValueNode * first_node, JsonValue *value, AlokArena *arena ) {
    if (!value) return nullptr;
    JsonValueNode * new_node = (JsonValueNode*) alok_arena_alloc(arena, sizeof(JsonValueNode) );
    new_node->value = value;
    new_node->next = first_node;
    return new_node;
}


static JsonValue * pvt_parse_array(JsonContext *context, JsonParseError *error, AlokArena *arena) {
    // we recursively parse elements of this array until we see end-of-array ']' char or run out of text
    // todo refactor to use stack data structure and iterative algorithm instead of recursion
    context->depth_current++;
    if ( pvt_max_depth_exceeded(context, error)) {
        context->depth_current--;
        return nullptr;  // error-out immediately
    }
    Input *input = context->input;

    JsonValue *array =  alok_arena_alloc(arena, sizeof(JsonValue) );

    size_t num_elements = 0;
    array->type = JSON_ARRAY;
    array->u.array.count = num_elements;
    array->u.array.elements = nullptr;

    JsonValueNode *elements = nullptr;
    pvt_advance(context, 1);  // consume '['
    pvt_skip_whitespace(context);

    //parse first element, then parse (comma, element)s until ']' or EOF
    if ( pvt_current_char(context) && pvt_current_char(context) != ']') {
        JsonValue *value = pvt_parse_value(context, error, arena);
        if (!value) {
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

    while ( pvt_current_char(context) && pvt_current_char(context) != ']' ) {
        pvt_skip_whitespace(context);
        //expect ','
        if (pvt_current_char(context) != ',' ) {
            pvt_record_missing_comma_error(context, error);
            context->depth_current--;
            return nullptr;
        }
        // comma is expected delimiter between array elements
        uint32_t comma_index = input->current_byte_index; // save for error reporting below
        pvt_advance(context, 1);  // consume ','
        pvt_skip_whitespace(context);

        if (pvt_current_char(context) == ']') {
            // we have a comma without another value
            if (jsonp_is_context_config_flag_set(context, JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_ARRAYS)) {
                // this allowed
                break;
            }
            // this is an error
            input->parse_start = comma_index;
            pvt_record_error(context, error, JSON_ERR_TRAILING_COMMA_NOT_ALLOWED,
                "trailing comma not allowed in array element list");
            // adjust error reporting so we indicate that the comma is the bad character
            error->first_bad_char = comma_index;
            error->column = comma_index;
            error->lab_offset = input->current_byte_index - comma_index;

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

    if (pvt_current_char(context) != ']' ) {
        pvt_record_error(context, error, JSON_ERR_UNTERMINATED_ARRAY, "missing closing bracket ']'");
        context->depth_current--;
        return nullptr;
    }
    pvt_advance(context, 1);  // consume ']'
    pvt_skip_whitespace(context);
    JsonValue **element_array = alok_arena_alloc(arena, sizeof(JsonValue) *  num_elements);
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

static constexpr uint8_t continue_mask = 0b00111111;  // 0x3F
static constexpr uint8_t continue_bits = 0b10000000;  // 0x80


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


static StringBuilder * pvt_encode_utf8(const JsonContext *context, JsonParseError *error, const uint32_t codepoint, StringBuilder *sb) {
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
            pvt_record_error(context, error, JSON_ERR_RESERVED_FOR_HIGH_SURROGATE, error->message);
            return nullptr;
        } else {
            // low/ trailing surrogate
            snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
        "'U+%.4X' is reserved as a low/trailing surrogate and cannot be used as a codepoint", codepoint);
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
        pvt_record_error(context, error, JSON_ERR_CODEPOINT_OUT_OF_RANGE, error->message);
        return nullptr;
    }

    return sb;
}

static uint32_t pvt_parse_hex_impl(const JsonContext *context, JsonParseError *error, const uint32_t num_chars) {
    uint32_t result = 0;
    for (uint32_t i = 0; i < num_chars; i++) {
        const char next_char = pvt_current_char(context);
        if (next_char == NUL) {
            snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
                "unexpected EOF while parsing hex digit. (expected %d hex digits, got %d)",  num_chars, i);
            pvt_record_error(context, error, JSON_ERR_UNEXPECTED_EOF, error->message);
            //  clang/clion linter doesn't see that record_error changes err_type.
            //   without this, it erroneously reports of unreachable code in calling methods
            error->err_type = JSON_ERR_UNEXPECTED_EOF;
            return 0;
        }
        if (!isxdigit((unsigned char)next_char)) {
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
static uint32_t pvt_parse_hex6(const JsonContext *context, JsonParseError *error) {
    return pvt_parse_hex_impl(context, error, 6);
}

// try to parse 4 hex bytes from the stream. Report in error if we didn't find 4 hex bytes.
static uint16_t pvt_parse_hex4(const JsonContext *context, JsonParseError *error) {
    return pvt_parse_hex_impl(context, error, 4);
}

//  expect a valid continuation byte from 0x80 - 0xBF at current index
// Return true if found, otherwise return false and report error
static bool pvt_validate_utf8_continuation_byte(
    const JsonContext *context, JsonParseError *error,
    uint8_t current_byte, uint8_t start_range, uint8_t end_range) {

    uint8_t next_byte = pvt_current_char(context);

    if ( !( next_byte >= start_range && next_byte <= end_range )) {
        snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
    "'0x%.2X' is an invalid UTF-8 continuation byte after '0x%.2X'", next_byte, current_byte);
        pvt_record_error(context, error, JSON_ERR_INVALID_UTF8_CONTINUATION_BYTE, error->message);
        return false;
    }

    return true;
}

static bool pvt_validate_utf8(const JsonContext *context, JsonParseError *error,  StringBuilder *sb) {
    uint8_t stream_bytes[4] = {};
    uint8_t lead_byte = pvt_current_char(context);;
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
        current_byte = (uint8_t)pvt_current_char(context);
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
            current_byte = (uint8_t)pvt_current_char(context);
            if ( current_byte >= 0x80 && current_byte <= 0x9F) {
                snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
            "'0x%.2X' is an invalid UTF-8 continuation byte and an overlong sequence", current_byte);
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
            current_byte = (uint8_t)pvt_current_char(context);
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
            current_byte = (uint8_t)pvt_current_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_current_char(context);
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
            current_byte = (uint8_t)pvt_current_char(context);
            if ( current_byte >= 0xA0 && current_byte <= 0xAF) {
                snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
            "'0x%.2X' is an invalid UTF-8 continuation byte and reserved for high surrogates", current_byte);
                pvt_record_error(context, error, JSON_ERR_RESERVED_FOR_HIGH_SURROGATE, error->message);
                return false;
            }
            if ( current_byte >= 0xB0 && current_byte <= 0xBF) {
                snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
            "'0x%.2X' is an invalid UTF-8 continuation byte and reserved for low surrogates", current_byte);
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
            current_byte = (uint8_t)pvt_current_char(context);
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
            current_byte = (uint8_t)pvt_current_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_current_char(context);
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
            current_byte = (uint8_t)pvt_current_char(context);
            if ( current_byte >= 0x80 && current_byte <= 0x8F) {
                snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
            "'0x%.2X' is an invalid UTF-8 continuation byte and an overlong sequence", current_byte);
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
            current_byte = (uint8_t)pvt_current_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_current_char(context);
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
            current_byte = (uint8_t)pvt_current_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_current_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_current_char(context);
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
            current_byte = (uint8_t)pvt_current_char(context);
            if ( current_byte >= 0x90 ) {
                snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
            "'0x%.2X' is an invalid UTF-8 continuation byte and is out of range", current_byte);
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
            current_byte = (uint8_t)pvt_current_char(context);
            stream_bytes[num_bytes++] = current_byte;
            pvt_advance(context, 1);
            if ( !pvt_validate_utf8_continuation_byte(context, error, lead_byte, 0x80, 0xBF )) {
                return false;
            }
            current_byte = (uint8_t)pvt_current_char(context);
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
        pvt_record_error(context, error, JSON_ERR_OVERLONG_SEQUENCE, error->message);
        return false;
    }

    snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
"'0x%.2X' is an invalid UTF-8 start byte", lead_byte);
    pvt_record_error(context, error, JSON_ERR_INVALID_UTF8_START_BYTE, error->message);
    return false;
}

// assumes pvt_peek_char(context) == 'u' or 'U' and the previous character was a backslash '\'
static StringBuilder * pvt_parse_unicode_escape(const JsonContext *context, JsonParseError *error, StringBuilder *sb_out ) {
    Input *input = context->input;

    if (pvt_current_char(context) == 'U') {
        if ( !jsonp_is_context_config_flag_set(context, JSON_CONFIG_ALLOW_UNICODE_U_ESCAPE)) {
            // got a \U (uppercase U) Unicode escape but flag is not enabled
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
        uint32_t bad_digit_index = input->current_byte_index - 6; // puts error char at \*
        pvt_record_error(context, error, JSON_ERR_NO_PRECEDING_HIGH_SURROGATE, error->message);
        error->first_bad_char = bad_digit_index;
        error->column = bad_digit_index;
        error->lab_offset = input->current_byte_index - bad_digit_index;
        return nullptr;
    }
    uint32_t cp = cp1;

    if (cp1 >= 0xD800 && cp1 <= 0xDBFF ) {
        // High surrogate, expect the low surrogate to follow.
        // RFC 8259: To escape an extended character (>U+FFFF), it must be
        // represented as a 12-character sequence encoding the UTF-16 surrogate pair.
        // Non-standard escapes like \UXXXXXXXX are not supported.
        const char current_char = pvt_current_char(context);

        // Check for enough remaining characters safely
        if (current_char != '\\' || pvt_peek_next_char(context) != 'u') {
            // no following low surrogate.
            snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
                "expected low surrogate escape \\uDC00-\\uDFFF to follow high surrogate '\\u%4X'", cp1);
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
                "expected low surrogate escape \\uDC00-\\uDFFF to follow high surrogate '\\u%4X', but found '\\u%4X' instead", cp1, cp2);
            uint32_t bad_digit_index = input->current_byte_index - 4; // puts error char at \u*

            // context->parse_start = bad_digit_index; // todo should parse_start be set to the start of the first surrogate escape??
            pvt_record_error(context, error, JSON_ERR_NO_FOLLOWING_LOW_SURROGATE, error->message);
            // first bad char is the digit after \uD, we update the error for more accurate reporting
            // context->current_index is the first digit after the second 4 hex , e.g., \uD800*
            error->first_bad_char = bad_digit_index;
            error->column = bad_digit_index;
            error->lab_offset = input->current_byte_index - bad_digit_index;

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

static constexpr char QUOTE           = 0x22;  // "
static constexpr char REVERSE_SOLIDUS = 0x5c;  // \  backslash

static bool pvt_parse_backlash_escapes(const JsonContext *context, JsonParseError *error, unsigned char current_byte, StringBuilder *sb ) {
        pvt_advance(context, 1); // Skip the backslash
        current_byte = (unsigned char )pvt_current_char(context);

        if (current_byte == NUL) {
            pvt_record_error(context, error, JSON_ERR_UNEXPECTED_EOF, "Unexpected EOF after backslash");
            return false; // Unexpected EOF
        }

        // Validate escape sequence
        switch (current_byte) {
            case '"':
            case '\\':
            case '/':
                sb_append_char(sb, (char)current_byte);
                pvt_advance(context, 1);
                break;
            case 'b':
                sb_append_char(sb, '\b');
                pvt_advance(context, 1);
                break;
            case 'f':
                sb_append_char(sb, '\f');
                pvt_advance(context, 1);
                break;
            case 'n':
                sb_append_char(sb, '\n');
                pvt_advance(context, 1);
                break;
            case 'r':
                sb_append_char(sb, '\r');
                pvt_advance(context, 1);
                break;
            case 't':
                sb_append_char(sb, '\t');
                pvt_advance(context, 1);
                break;
            case 'u':
            case 'U':
                // RFC 8259: \u followed by 4 hex digits
                // roblib addition \U followed by 6 hex digits is a codepoint,
                //  no surrogates required!
                StringBuilder *result = pvt_parse_unicode_escape(context, error, sb);
                if (!result) {
                    // if `pvt_parse_unicode_escape` encountered an error, it will have reported it in `error`
                    return false;
                }
                break;

            default:
                char const *format_str;
                if (current_byte < 0x20 || current_byte > 0x7E) format_str = "invalid escape sequence: '\\0x%.2X'";
                else format_str = "invalid escape sequence: '\\%c'";
                snprintf(error->message, ERROR_MSG_BUFFER_SIZE, format_str, current_byte);
                pvt_record_error(context, error, JSON_ERR_INVALID_ESCAPE_SEQUENCE, error->message);
                return false;
        }

    return true;
}



static JsonValue * pvt_parse_string(const JsonContext *context, JsonParseError *error, AlokArena *arena ) {

    AlokArenaTemp arena_temp = alok_arena_get_scratch(&(AlokArena*){arena}, 0);
    AlokArena * scratch_arena = arena_temp.arena;
    StringBuilder *sb2 = sb_new(16, scratch_arena);

    // we examine chars in the stream until we find:
    // 1. a closing quote, which is a quote not preceded by the backlash (reverse solidus)
    // 2. EOF, AKA null terminator, \x00
    JsonValue *value = nullptr;
    pvt_advance(context, 1); // Skip the opening quote
    unsigned char current_byte = (unsigned char )pvt_current_char(context);
    while (current_byte) {
        if (current_byte == QUOTE) {
            // happy case. We found the terminating quote
            value = alok_arena_alloc(arena, sizeof(JsonValue) );
            value->type = JSON_STRING;

            // Ensure the result is null-terminated from the StringBuilder
            // before copying into the arena.
            size_t len = sb2->length;

            char * str_value = alok_arena_alloc(arena, len + 1);
            memcpy(str_value, sb2->buffer, len);
            str_value[len] = NUL;

            value->u.string = (StringSlice){ .length = len, .data = str_value };

            pvt_advance(context, 1); // consume the terminating quote
            alok_arena_release_scratch(&arena_temp);
            return value;
        }

        if (current_byte == REVERSE_SOLIDUS) {
            if (!pvt_parse_backlash_escapes(context, error, current_byte, sb2)) {
                alok_arena_release_scratch(&arena_temp);
                return nullptr;
            }
        } else if ( current_byte <= 0x1F) {
            // RFC 8259: Control characters U+0000 through U+001F MUST be escaped. (u-escaped, not solidus-escaped)
            // This means the literal bytes cannot appear here.
            snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "unescaped control character: 0x%.2X", current_byte);
            pvt_record_error(context, error, JSON_ERR_UNESCAPED_CONTROL_CHAR, error->message);
            alok_arena_release_scratch(&arena_temp);
            return nullptr;
        } else {
            // Here we validate a string of UTF-8 characters
            if (! pvt_validate_utf8(context, error, sb2)) return nullptr;
        }
        current_byte = (unsigned char)pvt_current_char(context);
    }  // end while (current_byte)
    pvt_record_error(context, error, JSON_ERR_UNTERMINATED_STRING, "missing closing quote '\"' ");
    alok_arena_release_scratch(&arena_temp);

    return nullptr;
}

// ReSharper disable once CppUseInternalLinkage
typedef struct CharBuffer {
    size_t length;
    uint8_t buffer[1024];
} CharBuffer;

static void pvt_add_char_to_buffer( CharBuffer * cb, char c) {
    if (cb->length > sizeof (cb->buffer) - 1) return;

    cb->buffer[cb->length++] = (uint8_t)c;
}

static JsonValue * pvt_parse_number(const JsonContext *context, JsonParseError *error, AlokArena *arena ) {
    JsonValue *value = nullptr;
    CharBuffer cb = {}; // todo this is a fixed buffer of 1024 bytes. Max length of number we can parse.


    //context->parse_start has the first character in this number string, which we'll need to convert the parsed number
    char cur_char = pvt_current_char(context);
    if (cur_char == '.') {
        snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
    "expected digit '0'-'9' before the decimal point");
        pvt_record_error(context, error, JSON_ERR_INVALID_NUMBER_FORMAT, error->message);
        return nullptr;
    }

    if (cur_char == '-') {
        pvt_add_char_to_buffer(&cb, cur_char);
        pvt_advance(context, 1);  // optional '-' is fine.
        //must be followed by a digit
        char c = pvt_current_char(context);
        if ( !(c >= '0' && c <= '9')) {
            int written =  snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
        "expected digit '0'-'9' after '-', got ");
            if (written > 0) {
                pvt_format_error_message_char(ERROR_MSG_BUFFER_SIZE - written,
                    error->message + written, pvt_current_char(context) );
            }
            pvt_record_error(context, error, JSON_ERR_INVALID_NUMBER_FORMAT, error->message);
            return nullptr;
        }
    }

    // Number *must* start with 0 exactly once, or a digit 1-9 exactly once, followed by zero or more digits 0-9
    cur_char = pvt_current_char(context);
    if ( !( cur_char >= '0' && cur_char <= '9')) {
        int written =  snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
            "numbers must start with a digit '0'-'9', got ");
        if (written > 0) {
            pvt_format_error_message_char(ERROR_MSG_BUFFER_SIZE - written,
                error->message + written, pvt_current_char(context) );
        }
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
        char c = pvt_current_char(context);
        // leading zero only valid if next character is a period, e or E
        if ( c >= '0' && c <= '9') {
            int written =  snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
                "invalid leading zero. expected '.', 'e', or 'E', got ");
            if (written > 0) {
                pvt_format_error_message_char(ERROR_MSG_BUFFER_SIZE - written,
                    error->message + written, pvt_current_char(context) );
            }
            pvt_record_error(context, error, JSON_ERR_INVALID_NUMBER_FORMAT, error->message);
            return nullptr;
        }
        // leading zero only valid if next character is a period, e or E and not a digit
        if (  c != '.' && c != 'e' && c != 'E' ) {
            // we parsed a zero
            value = alok_arena_alloc(arena, sizeof(JsonValue) );
            value->type = JSON_LONG;
            value->u.n_long = 0;
            return value;
        }
    } else {
        // cur_char '1'-'9'
        while ( pvt_current_char(context) >= '0' && pvt_current_char(context) <= '9' ) {
            pvt_add_char_to_buffer(&cb, pvt_current_char(context));
            pvt_advance(context, 1);
        }
    }

    // -----------------------------------------------------------------
    //          CONVERT NUMBER TO INTEGER
    // -----------------------------------------------------------------

    cur_char = pvt_current_char(context);
    if ( cur_char != '.' && cur_char != 'e' && cur_char != 'E') {
        // we parsed an integer
        value = alok_arena_alloc(arena, sizeof(JsonValue) );
        value->type = JSON_LONG;
        errno = 0; // Reset errno before the calls
        char *str_end =  nullptr;
        long val = strtol( (char const *)cb.buffer, &str_end, 10);

        if ( errno == 0) {
            value->u.n_long = val;
        } else if (errno == ERANGE) {
            // Promotion: If too big for long, use double to preserve magnitude (even if it becomes Infinity)
            // todo (rob) warn? exit with error depending on config flag?
            fprintf(stderr, "warning: number too large to parse as integer: %s\n", (char const *)cb.buffer);
            errno = 0;
            value->u.n_double = strtod((char const *)cb.buffer, &str_end);
            if (errno) {
                value->u.n_long = 0;
                fprintf(stderr, "warning: encountered errno %d: %s, while converting large integer to floating point: %s\n",
                   errno, strerror(errno), (char const *)cb.buffer);
            } else {
                value->type = JSON_DOUBLE;
                fprintf(stderr, "info:    successfully converted large integer: %s to floating point: %g\n",
                    (char const *)cb.buffer, value->u.n_double);
            }
        } else {
            // what other errors are possible here?
            value->u.n_long = 0;
            fprintf(stderr, "error: errno %d: %s, while converting number to long int: %s\n",
                errno, strerror(errno), (char const *)cb.buffer);
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
        char c = pvt_current_char(context);
        if ( !(c >= '0' && c <= '9') ) {
            int written =  snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "expected digit '0'-'9' after the decimal point, got ");
            if (written > 0) {
                pvt_format_error_message_char(ERROR_MSG_BUFFER_SIZE - written,
                    error->message + written, pvt_current_char(context) );
            }
            pvt_record_error(context, error, JSON_ERR_INVALID_NUMBER_FORMAT, error->message);
            return nullptr;
        }
        pvt_add_char_to_buffer(&cb, c);
        pvt_advance(context, 1); // consume first digit

        // consume next N digits
        while ( pvt_current_char(context) >= '0' && pvt_current_char(context) <= '9' ) {
            pvt_add_char_to_buffer(&cb, pvt_current_char(context));
            pvt_advance(context, 1);
        }
    }

    cur_char = pvt_current_char(context);
    if ( cur_char == 'e' || cur_char == 'E' ) {
        char const e_char = cur_char;  // save actual letter case for reporting
        pvt_add_char_to_buffer(&cb, cur_char);
        pvt_advance(context, 1);
        char c = pvt_current_char(context);
        if ( c == '-' || c == '+' ) {
            pvt_add_char_to_buffer(&cb, c);
            pvt_advance(context, 1); // consume optional -/+ char
        }
        // expect one number
        c = pvt_current_char(context);
        if ( !(c >= '0' && c <= '9')) {
            int written =  snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "expected digit '0'-'9' after '%c', got ", e_char);
            if (written > 0) {
                pvt_format_error_message_char(ERROR_MSG_BUFFER_SIZE - written,
                    error->message + written, pvt_current_char(context) );
            }
            pvt_record_error(context, error, JSON_ERR_INVALID_NUMBER_FORMAT, error->message);
            return nullptr;
        }
        pvt_add_char_to_buffer(&cb, c);
        pvt_advance(context, 1); // consume first digit
        // consume next N digits
        while ( pvt_current_char(context) >= '0' && pvt_current_char(context) <= '9') {
            pvt_add_char_to_buffer(&cb, pvt_current_char(context) );
            pvt_advance(context, 1);
        }
    }

    // -----------------------------------------------------------------
    //          CONVERT NUMBER AS FLOATING POINT
    // -----------------------------------------------------------------

    // we've now parsed a float number
    value = alok_arena_alloc(arena, sizeof(JsonValue) );
    value->type = JSON_DOUBLE;
    errno = 0; // Reset errno before the calls
    char *str_end =  nullptr;
    value->u.n_double = strtod((char const *)cb.buffer, &str_end);

    // Note: If strtod overflows, u.n_double will be +/- Infinity (HUGE_VAL).
    if ( errno == ERANGE) {
        fprintf(stderr, "warning: double float out of range. converted as: %g, number was: %s\n",
            value->u.n_double, (char const *)cb.buffer );
    }
    else if ( errno != 0 ) {
        value->u.n_double = 0.0;
        fprintf(stderr, "error: errno %d: %s, while converting number to floating point. Converted as: 0.0. Number was: %s\n",
            errno, strerror(errno), (char const *)cb.buffer);
    }

    return value;
}


// not used, but this defines what a JSON number is, so it's convenient to have around for reference.
[[maybe_unused]] static const char * const REGEX_NUMBER_STR = "(-?(0|([1-9][0-9]*))(\\.[0-9]+)?([eE][-+]?[0-9]+)?)";

static JsonValue *  pvt_parse_literal_impl(const JsonContext *context,
                                                JsonParseError *error,
                                                JsonValue *literal,
                                                char const *key_word ) {

    char const * keyword_ptr = key_word;
    char cur_char = pvt_current_char(context);
    enum json_error_type_e err_type = JSON_ERR_NONE;

    while ( *keyword_ptr != NUL && cur_char != NUL) {
        if (*keyword_ptr != cur_char) {
            err_type = JSON_ERR_UNEXPECTED_TEXT;
            snprintf(error->message, ERROR_MSG_BUFFER_SIZE,
                "unexpected character: '%c', expected '%s'", cur_char, key_word);
            break;
        }
        pvt_advance(context, 1);
        cur_char = pvt_current_char(context);
        keyword_ptr++;
    }

    if ( err_type == JSON_ERR_NONE && *keyword_ptr != NUL) {
        // unexpected end of text; the stream ended before we finished parsing the keyword
        err_type = JSON_ERR_UNEXPECTED_EOF;
        snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "unexpected EOF, expected '%s'", key_word);
    }
    if (err_type != JSON_ERR_NONE) {
        pvt_record_error(context, error, err_type, error->message);
        return nullptr;
    }

    return literal;
}

static constexpr StringSlice NULL_SLICE  = (StringSlice){ .length = 4, .data = "null"};




// todo (rob) these JsonValues need to be const
static constexpr size_t JSON_KEYWORD_NULL_LEN = 4;
static constexpr char   JSON_KEYWORD_NULL[JSON_KEYWORD_NULL_LEN + 1] = "null";
static JsonValue JSON_NULL_VALUE = { .type = JSON_NULL, .u.string = NULL_SLICE};

static constexpr size_t JSON_KEYWORD_TRUE_LEN = 4;
static constexpr char   JSON_KEYWORD_TRUE[JSON_KEYWORD_TRUE_LEN + 1] = "true";
static JsonValue JSON_TRUE_VALUE = { .type = JSON_BOOLEAN, .u.boolean = true};

static constexpr size_t JSON_KEYWORD_FALSE_LEN = 5;
static constexpr char   JSON_KEYWORD_FALSE[JSON_KEYWORD_FALSE_LEN + 1] = "false";
static JsonValue JSON_FALSE_VALUE = { .type = JSON_BOOLEAN, .u.boolean = false};

static JsonValue *  pvt_parse_true(const JsonContext *context, JsonParseError *error ) {
    return pvt_parse_literal_impl(context, error, &JSON_TRUE_VALUE, JSON_KEYWORD_TRUE );
}

static JsonValue *  pvt_parse_false(const JsonContext *context, JsonParseError *error ) {
    return pvt_parse_literal_impl(context, error, &JSON_FALSE_VALUE, JSON_KEYWORD_FALSE );
}

static JsonValue *  pvt_parse_null(const JsonContext *context, JsonParseError *error ) {
    return pvt_parse_literal_impl(context, error, &JSON_NULL_VALUE, JSON_KEYWORD_NULL );
}

static JsonValue *pvt_parse_value(JsonContext *context, JsonParseError *error, AlokArena *arena ) {
    Input *input = context->input;

    pvt_skip_whitespace(context);
    input->parse_start = input->current_byte_index;
    JsonValue *value = nullptr;

    switch (pvt_current_char(context)) {
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
            int written = snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "unexpected character: ");
            if (written > 0) {
                pvt_format_error_message_char(ERROR_MSG_BUFFER_SIZE - written,
                    error->message + written, pvt_current_char(context) );
            }
            pvt_record_error(context, error, JSON_ERR_UNEXPECTED_TEXT, error->message);
            return nullptr;

            break;

    }

    return value;
}

static JsonValue * pvt_jsonp_parse_impl(JsonContext *context, JsonParseError *error, AlokArena *arena) {
    Input * input = context->input;

    if (!input->input_context) {
        *error = (JsonParseError){ .message = "null Input source", .err_type = JSON_ERR_NULL_TEXT};
        return nullptr;
    }

    int peek_char = input->current_char(input->input_context);
    if (peek_char == EOF) {
        *error = (JsonParseError){ .message = "empty json text", .err_type = JSON_ERR_EMPTY_TEXT};
        return nullptr;
    }

    if (pvt_is_rejected_due_to_bom(context,  error)) return nullptr;

    pvt_skip_whitespace(context);
    // todo (rob) this might need to compare to EOF not NUL for non-string sources?
    char current_char = pvt_current_char(context);
    if ( current_char == NUL || current_char == EOF) {
        pvt_record_error(context, error, JSON_ERR_EMPTY_TEXT, "json text is composed only of white space");
        return nullptr;
    }

    error->err_type = JSON_ERR_NONE;
    JsonValue *value =  pvt_parse_value(context, error, arena);
    if ( error->err_type != JSON_ERR_NONE) {
        return nullptr;
    }

    pvt_skip_whitespace(context);

    if (pvt_current_char(context) != EOF && pvt_current_char(context) != NUL) {
        //we parsed the root value, but there is still text remaining in the JSON text, which is an error
        pvt_record_error(context, error, JSON_ERR_UNEXPECTED_TEXT, "unexpected extra text after parsing a valid JSON value");
        return nullptr;
    }
    return value;
}



// -----------------------------------------------------------------
//          BOM CHECKING
// -----------------------------------------------------------------

static bool pvt_starts_with_bom(JsonContext const *context,
                                const uint32_t n_bytes,
                                uint8_t const bom_bytes[static n_bytes]) {

    Input *input = context->input;
    for (uint32_t i = 0; i < n_bytes; ++i) {
        if ( input->peek_lookahead_chars(input->input_context, i) != (unsigned char)bom_bytes[i]) {
            return false;
        }
    }
    return true;
}

static bool pvt_is_rejected_due_to_bom(const JsonContext *context, JsonParseError *error) {
    // UTF-16 and UTF-32 BOMs always cause failure
    uint32_t bom_array_length = ARRAY_COUNT(BOM_UTF16_BE);
    if (pvt_starts_with_bom( context ,bom_array_length, BOM_UTF16_BE) ||
        pvt_starts_with_bom( context, bom_array_length, BOM_UTF16_LE)) {
                pvt_advance(context, bom_array_length);
                pvt_record_error(context, error, JSON_ERR_UNEXPECTED_UTF16_ENCODING, "This document appears to be encoded with UTF-16. Expected UTF-8.");
                return true;
            }
    bom_array_length = ARRAY_COUNT(BOM_UTF32_BE);
    if (pvt_starts_with_bom(context, bom_array_length, BOM_UTF32_BE) ||
        pvt_starts_with_bom(context, bom_array_length, BOM_UTF32_LE)) {
                pvt_advance(context, bom_array_length);
                pvt_record_error(context, error, JSON_ERR_UNEXPECTED_UTF32_ENCODING, "This document appears to be encoded with UTF-32. Expected UTF-8.");
                return true;
            }

    // utf8 BOM behavior controlled by flag JSON_CONFIG_FAIL_ON_INITIAL_BOM
    bom_array_length = ARRAY_COUNT(BOM_UTF8);
    if (pvt_starts_with_bom(context, bom_array_length, BOM_UTF8)  ) {
        if (jsonp_is_context_config_flag_set(context, JSON_CONFIG_FAIL_ON_INITIAL_BOM)) {
            pvt_advance(context, bom_array_length);
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
static char pvt_get_locale_decimal_separator_char() {
    struct lconv *lc = localeconv();
    if (lc && lc->decimal_point) {
        unsigned char c = (unsigned char) lc->decimal_point[0];
        if ( c > 0x20 && c < 0x7F) {
            return (char)c;
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

char jsonp_get_context_decimal_separator(const JsonContext *context ) {
    return context->decimal_separator;
}

void jsonp_set_context_decimal_separator( JsonContext *context, char c) {
    context->decimal_separator = c;
}

static void pvt_init_context_whitespace_table(JsonContext *context) {
    const char *ws = context->whitespace_chars;
    // create lookup table for O(1) lookups for pvt_is_json_whitespace()
    memset(context->ws_table, 0, sizeof(context->ws_table));
    while (*ws) {
        context->ws_table[(unsigned char)*ws++] = true;
    }
}

static void pvt_copy_global_state(JsonContext *context) {
    if (!context) return;
    context->config_flags = atomic_load(&json_config_flags);
    context->depth_max    = atomic_load(&pvt_depth_max);
    const char *ws        = atomic_load(&pvt_whitespace_chars);
    snprintf(context->whitespace_chars, sizeof(context->whitespace_chars), "%s", ws);
    pvt_init_context_whitespace_table(context);
    context->decimal_separator = pvt_get_locale_decimal_separator_char();

}

JsonContext *jsonp_copy_global_context() {
    JsonContext *context  = (JsonContext *)calloc(1, sizeof(JsonContext));
    pvt_copy_global_state(context);
    return context;
}

JsonContext *jsonp_make_empty_context() {
    JsonContext *context  = (JsonContext *)calloc(1, sizeof(JsonContext));
    context->whitespace_chars[0] = NUL; // empty string
    return context;
}


// reset to initial states all state-related members of the context.
// Does not affect depth_max, config_flags, whitespace_chars, nor ws_table.
static void pvt_reset_context(JsonContext *context) {
    context->depth_current  = 0;
    context->input          = nullptr;
    memset(context->error_msg, NUL, ERROR_MSG_BUFFER_SIZE + 1 );
}


static Input pvt_get_string_input_source( StringSourceInputContext *ss) {
    Input input = {
        .input_context            = (void*)ss,
        .read                     = string_read,
        .current_char             = string_current_char,
        .peek_next_char           = string_peek_next_char,
        .peek_lookahead_chars     = string_peek_lookahead_chars,
        .advance_n_bytes          = string_advance_n_bytes,
        .sprint_n_lookahead_chars = string_sprint_n_lookahead_chars
    };
    // ss->look_behind_buffer = &input.look_behind_buffer;
    return input;
}

static bool pvt_json_text_is_null_or_empty(const char *json_text, JsonParseError *error) {
    if (!json_text) {
        *error = (JsonParseError){ .message = "null json text", .err_type = JSON_ERR_NULL_TEXT};
        return true;
    }
    if (json_text[0] == NUL) {
        *error = (JsonParseError){ .message = "empty json text", .err_type = JSON_ERR_EMPTY_TEXT};
        return true;
    }
    return false;
}


JsonValue *jsonp_parse_string_using_context(const char *json_text, JsonParseError *error, AlokArena *arena, JsonContext *context ) {
    pvt_reset_context(context);
    if (pvt_json_text_is_null_or_empty(json_text, error)) return nullptr;

    StringSourceInputContext ss = { .json_text = json_text, .length_bytes = strlen(json_text) };
    Input input = pvt_get_string_input_source(&ss);
    ss.input = &input;
    CharRingBuffer *crb = crb_new_CharRingBuffer(LOOK_AHEAD_BUF_SIZE, arena);
    input.look_behind_buffer = crb;
    context->input = &input;

    return pvt_jsonp_parse_impl(context, error, arena);
}


JsonValue * jsonp_parse_string(const char *json_text, JsonParseError *error, AlokArena *arena) {
    if (pvt_json_text_is_null_or_empty(json_text, error)) return nullptr;

    StringSourceInputContext ss = { .json_text = json_text, .length_bytes = strlen(json_text) };
    Input input = pvt_get_string_input_source( &ss );
    ss.input = &input;

    CharRingBuffer *crb = crb_new_CharRingBuffer(LOOK_AHEAD_BUF_SIZE, arena);
    input.look_behind_buffer = crb;
    JsonContext context = {};
    pvt_copy_global_state(&context);
    context.input = &input;
    JsonValue *value = pvt_jsonp_parse_impl(&context, error, arena);

    return value;
    // return jsonp_parse_string_impl(&input, error, arena);
}



JsonValue *jsonp_parse_string_ex(const char *json_text, JsonParseError *error, AlokArena *arena, const uint32_t buffer_size) {
    if (pvt_json_text_is_null_or_empty(json_text, error)) return nullptr;

    StringSourceInputContext ss = { .json_text = json_text, .length_bytes = buffer_size };
    Input input = pvt_get_string_input_source( &ss);
    ss.input = &input;
    CharRingBuffer *crb = crb_new_CharRingBuffer(LOOK_AHEAD_BUF_SIZE, arena);
    input.look_behind_buffer = crb;
    JsonContext context = {};
    pvt_copy_global_state(&context);
    context.input = &input;

    JsonValue *value = pvt_jsonp_parse_impl(&context, error, arena);
    if (!value) return nullptr;

    if (input.current_byte_index < buffer_size) {
        snprintf(context.error_msg, ERROR_MSG_BUFFER_SIZE,
    "JSON text was successfully parsed, but unparsed characters remain in the buffer."
    " This can happen when the text is followed by embedded NUL characters.\nbuffer size=%u, bytes parsed=%u",
    buffer_size, input.current_byte_index );
        pvt_record_error(&context, error, JSON_ERR_EXPECTED_EOF, context.error_msg);
        return nullptr;

    }
    return value;
}

static Input pvt_get_file_input_source( FileSourceInputContext *fs) {
    Input input = {
        .input_context            = (void*)fs,
        .read                     = file_read,
        .current_char             = file_current_char,
        .peek_next_char           = file_peek_next_char,
        .peek_lookahead_chars     = file_peek_lookahead_chars,
        .advance_n_bytes          = file_advance_n_bytes,
        .sprint_n_lookahead_chars = file_sprint_n_lookahead_chars
    };
    // fs->look_behind_buffer = &input.look_behind_buffer;

    return input;
}

static void pvt_debug_file_buffering(FILE *fp) {
#if defined(__APPLE__) || defined(__FreeBSD__)
    // On macOS/BSD, we can inspect the internal __sFILE structure
    struct __sFILE *internal = (struct __sFILE *)fp;

    const char *mode = "Fully-buffered";
    if (internal->_flags & 0x0002) mode = "Unbuffered";
    else if (internal->_flags & 0x0001) mode = "Line-buffered";

    printf("--- stdio Debug ---\n");
    printf("Buffering Mode: %s\n", mode);
    //prime the buffer
    char c;
    size_t num_read = fread(&c, 1, 1, fp);
    int fseek_result = fseek( fp,  0, SEEK_SET );  // try to seek to start  to unread the char

    printf("Internal Buffer Size: %d bytes\n", internal->_bf._size);
#endif

    // POSIX way to see what the OS prefers
    struct stat st;
    if (fstat(fileno(fp), &st) == 0) {
        printf("FS Optimal Block Size: %d bytes\n", st.st_blksize);
    }

    // In many glibc/Linux environments, you'd use <stdio_ext.h>
    // printf("Glibc buffer size: %zu\n", __fbufsize(fp));
}

JsonValue * jsonp_parse_file(const char *json_filename, JsonParseError *error, AlokArena *arena) {
    if (!json_filename) {
        *error = (JsonParseError){ .message = "JSON filename is a nullptr", .err_type = JSON_ERR_NULL_TEXT};
        return nullptr;
    }
    if (json_filename[0] == NUL) {
        *error = (JsonParseError){ .message = "JSON filename is empty string", .err_type = JSON_ERR_EMPTY_TEXT};
        return nullptr;
    }

    // Check if file exists and is accessible before attempting to open
    struct stat st;
    if (stat(json_filename, &st) != 0) {
        int saved_errno = errno;
        if (saved_errno == ENOENT) {
            snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "File '%s' not found.", json_filename);
            error->err_type = JSON_ERR_FILE_NOT_FOUND;
        } else if (saved_errno == EACCES) {
            snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "Permission denied for file '%s': %s", json_filename, strerror(saved_errno));
            error->err_type = JSON_ERR_FILE_ACCESS_ERROR;
        } else {
            snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "Error accessing file '%s': %s", json_filename, strerror(saved_errno));
            error->err_type = JSON_ERR_FILE_ACCESS_ERROR;
        }
        return nullptr;
    }

    // todo (rob) check if it's a regular file (S_ISREG(st.st_mode))

    FILE *fp = fopen(json_filename, "rb");
    int saved_errno = errno;

    if (!fp) {
        // handle error
        // If stat() succeeded but fopen() failed, it's likely a different issue (e.g., too many open files, file is a directory)
        snprintf( error->message, ERROR_MSG_BUFFER_SIZE, "Failed to open file '%s': %s", json_filename, strerror(saved_errno) );
        error->err_type = JSON_ERR_FILE_OPEN_FAILED;
        return nullptr;
    }

    // Check buffering before we do anything.
    // Note: Some implementations don't allocate the buffer until the first I/O call.
    // pvt_debug_file_buffering(fp);

    // need to get file size
    long file_size = 0;
    // let's try fseek
    if (fseek(fp, 0, SEEK_END) != 0) {
        saved_errno = errno;
        snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "fseek(SEEK_END) failed: %s", strerror(saved_errno));
        error->err_type = JSON_ERR_FILE_OPEN_FAILED;
        fclose(fp);
        return nullptr;
    }

    // if we got here, we were able to seek to the end.
    long tell_pos = ftell( fp );
    if (tell_pos < 0) {
        saved_errno = errno;
        snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "ftell failed: %s", strerror(saved_errno));
        error->err_type = JSON_ERR_FILE_OPEN_FAILED;
        fclose(fp);
        return nullptr;
    }
    file_size = tell_pos;

    if (fseek(fp, 0, SEEK_SET) != 0) {
        saved_errno = errno;
        snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "fseek(SEEK_SET) failed: %s", strerror(saved_errno));
        error->err_type = JSON_ERR_FILE_OPEN_FAILED;
        fclose(fp);
        return nullptr;
    }


    FileSourceInputContext fs = {
        .json_file_full_path = json_filename,
        .json_filename = json_filename,
        .file_ptr = fp,
        .length_bytes = file_size,
        .scanner_buffer = crb_new_CharRingBuffer(LOOK_AHEAD_BUF_SIZE, arena),
    };

    Input input = pvt_get_file_input_source(&fs);
    fs.input = &input;
    CharRingBuffer *crb = crb_new_CharRingBuffer(LOOK_AHEAD_BUF_SIZE, arena);
    input.look_behind_buffer = crb;
    JsonContext context = {};
    pvt_copy_global_state(&context);
    context.input = &input;

    JsonValue *value = nullptr;
    value = pvt_jsonp_parse_impl(&context, error, arena);


    int close_err = fclose(fp);
    fs.file_ptr = nullptr;
    // assigns no-op read function in case future calls to pvt_record_error() try to
    // read from the file which is now closed.
    input.read = string_read;

    if (error->err_type != JSON_ERR_NONE ) return nullptr;

    //check if there are still chars remaining that weren't parsed
    if ( fs.length_bytes > input.current_byte_index ) {
        snprintf(context.error_msg, ERROR_MSG_BUFFER_SIZE,
        "JSON text was successfully parsed, but unparsed characters remain in the buffer."
        " This can happen when the text is followed by embedded NUL characters.\nbuffer size=%zd, bytes parsed=%u",
        fs.length_bytes, input.current_byte_index );
        pvt_record_error(&context, error, JSON_ERR_EXPECTED_EOF, context.error_msg);
        return nullptr;
    }


    return value;
}

JsonValue * jsonp_parse_stream( FILE* fp, JsonParseError *error, AlokArena *arena) {
    if (!fp) {
        *error = (JsonParseError){ .message = "FILE pointer is nullptr", .err_type = JSON_ERR_NULL_TEXT};
        return nullptr;
    }

    // Try to determine size for buffering logic.
    // Note: This may fail for non-seekable streams like pipes/stdin.
    long current_pos = ftell(fp);
    long file_size = 0;
    if (current_pos != -1) {
        if (fseek(fp, 0, SEEK_END) == 0) {
            file_size = ftell(fp);
            fseek(fp, current_pos, SEEK_SET);
        }
    }
    // file_size = 0;  // simulate a sequential stream

    FileSourceInputContext fs = {
        .json_file_full_path = "stream",
        .json_filename = "stream",
        .file_ptr = fp,
        .length_bytes = file_size,
        .scanner_buffer = crb_new_CharRingBuffer(LOOK_AHEAD_BUF_SIZE, arena),
    };

    Input input = pvt_get_file_input_source(&fs);
    fs.input = &input;
    CharRingBuffer *crb = crb_new_CharRingBuffer(LOOK_AHEAD_BUF_SIZE, arena);
    input.look_behind_buffer = crb;
    JsonContext context = {};
    pvt_copy_global_state(&context);
    context.input = &input;

    JsonValue *value = nullptr;
    value = pvt_jsonp_parse_impl(&context, error, arena);


    fs.file_ptr = nullptr;
    // assigns no-op read function in case future calls to pvt_record_error() try to
    // read from the file which is now closed.
    input.read = string_read;


    return value;
}


// A struct to help us collect the data from libcurl
// ReSharper disable once CppUseInternalLinkage
struct MemoryStruct {
    char *memory;
    size_t size;
};

// This is a callback function that libcurl will call with chunks of data
static size_t
WriteMemoryCallback(const void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    // todo (rob) use arena
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

JsonValue * jsonp_parse_url( const char* url, JsonParseError *error, AlokArena *arena) {
    // todo (rob) curl is not installed by default. So this method should be in an API extension file that clients
    // can choose to use if they want to d/l, but otherwise have no dependency on it if not.
    CURL *curl_handle;
    CURLcode res;

    struct MemoryStruct chunk;
    // todo (rob) use arena
    chunk.memory = malloc(1);  /* will be grown as needed by the callback */
    chunk.size = 0;            /* no data at this point */

    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();

    // Set the URL to fetch
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    // Set the callback function to handle the data
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    // Pass our 'chunk' struct to the callback
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    // Some servers don't like requests that are not from a "browser"
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    // Perform the request
    res = curl_easy_perform(curl_handle);

    JsonValue* value = nullptr;
    if(res != CURLE_OK) {
        snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "curl_easy_perform() failed: %s", curl_easy_strerror(res));
        // TODO(rob): Add a new, more specific error type like JSON_ERR_NETWORK_ERROR
        error->err_type = JSON_ERR_FILE_ACCESS_ERROR;
    } else {
        // Now, parse the downloaded data from memory
        printf("Downloaded %zu bytes. Parsing...\n", chunk.size);
        // value = jsonp_parse_string(chunk.memory, error, arena);
        // jsonp_parse_string_ex is better here as it handles embedded nulls.
        value = jsonp_parse_string_ex(chunk.memory, error, arena, chunk.size);

        // Alternately, we could create an in-memory FILE* stream from the downloaded data
        // if the file is very large we could have a read and write buffer in a background thread to parse
        // as the file is being downloaded.
        // FILE* stream = fmemopen(chunk.memory, chunk.size, "rb");
        // if (stream == NULL) {
        //     snprintf(error->message, ERROR_MSG_BUFFER_SIZE, "fmemopen() failed.");
        //     error->err_type = JSON_ERR_FILE_OPEN_FAILED;
        // } else {
        //     // Now you can use your new function!
        //     value = jsonp_parse_stream(stream, error, arena);
        //     fclose(stream);
        // }
    }

    // Cleanup
    curl_easy_cleanup(curl_handle);
    // Free the temporary buffer used by the callback.
    free(chunk.memory);
    curl_global_cleanup();

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
Error (jsonp_init)(jp_bitset_t config_flags, uint32_t max_depth, char const * whitespace_chars) {


    //todo temp debug remove
    // pvt_tmp_set_local_decimal_separator_char(",");


    if (atomic_load(&is_initialized)) {
        return (Error){ .err = false };  // already initialized, no-op, empty error
    }

    // todo (rob) parsing a url should be in an extension unit and initialization should be handled separately
    // Initialize libcurl globally. This is thread-safe and safe to call multiple times,
    // but it's best practice to call it once at the start.
    // if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
    //     return (Error){ .err = true, .msg = "Failed to initialize libcurl" };
    // }

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


// -----------------------------------------------------------------
//      DESTROY
// -----------------------------------------------------------------

void jsonp_destroy() {
    if (atomic_exchange(&is_initialized, false)) {
        const char *ws = atomic_exchange(&pvt_whitespace_chars, nullptr);
        if (ws) {
            free( (void*)ws);
        }
        // Clean up libcurl resources.
        // curl_global_cleanup();
    }
}



// -----------------------------------------------------------------
//      Error Reporting
// -----------------------------------------------------------------


const char *jsonp_parse_error_type_name(const JsonParseErrType err_type) {
    switch (err_type) {
        /* 3. Expand the list to create the Switch Cases */
#define X(name) case JSON_ERR_##name: return #name;
        JSON_ERROR_LIST(X)
#undef X
    }
    return "UNKNOWN_JSON_ERROR";
}

static constexpr char NEWLINE_CHARS[] = "\n\r\t\f\v";
static constexpr char SPACE_CHAR = ' ';

static constexpr char caret[]       = "\033[91m^\033[0m";
static constexpr char left_caret[]  = "\033[91m>\033[0m";
static constexpr char right_caret[] = "\033[91m<\033[0m";

void jsonp_print_parse_error(JsonParseError *err) {
    if (!err) {
        printf("(JsonParseError)nullptr\n");
        return;
    }

    printf("%d:%s  line:%d col:%d pos:%d start:%d end:%d :  %s  \n",
    err->err_type, jsonp_parse_error_type_name(err->err_type),
    err->line+1, err->column+1, err->first_bad_char, err->parse_start, err->parse_end, err->message);

    // debug version
    // printf("%d:%s  line:%d col:%d pos:%d start:%d end:%d :  %s   lab offset:%d look_behind_buffer: '%s', look_ahead_buffer: '%s', err->json: '%s'\n",
    //     err->err_type, jsonp_parse_error_type_name(err->err_type),
    //     err->line+1, err->column+1, err->first_bad_char, err->parse_start, err->parse_end, err->message,
    //     err->lab_offset, err->look_behind_buffer, err->look_ahead_buffer, err->json);


    if ( err->err_type == JSON_ERR_NULL_TEXT ||
          ( err->err_type == JSON_ERR_EMPTY_TEXT && !strlen(err->look_behind_buffer)) ) {
        return; // nothing was scanned, i.e. null/empty text
    }

    uint32_t err_pos_offset = err->lab_offset;
    StringBuilder json_err_text = {};
    sb_init( &json_err_text, 80 + 20, err->look_behind_buffer);
    if (err_pos_offset > 0) {
        uint32_t insert_pos = json_err_text.length - err_pos_offset;
        sb_insert_str(&json_err_text,  right_caret, insert_pos + 1 );
        sb_insert_str(&json_err_text,  left_caret, insert_pos );
        sb_append_str(&json_err_text, err->look_ahead_buffer);
    } else {
        sb_append_str(&json_err_text, left_caret);
        sb_append_char(&json_err_text, *err->look_ahead_buffer);
        sb_append_str(&json_err_text, right_caret );
        sb_append_str(&json_err_text, err->look_ahead_buffer + 1);
    }

    // for display, replace new line chars with a space so the error message remains on the same line
    sb_replace_match_chars(&json_err_text,NEWLINE_CHARS, SPACE_CHAR );
    printf("%s\n", json_err_text.buffer);

    sb_destroy(&json_err_text);
}

//// ------------------------------------------------------------
////
////    TESTING
////
//// ------------------------------------------------------------


static void parse_test_str(char const * str) {
    Error init_err = jsonp_init();
    if (init_err.err) {
        err_print(init_err);
        jsonp_destroy();
        return;
    }
    ArenaErrResult aer =  alok_arena_create( ONE_MEBIBYTE * 100);
    AlokArena *arena = aer.result;
    if ( aer.err ) {
        printf("alok_arena_create failed with %d, %s\n", aer.reported_err, aer.msg);
        jsonp_destroy();
        return;
    }
    JsonParseError err = {};
    printf("\nParsing json string '%s': \n", str);
    JsonValue *jval = jsonp_parse_string(str, &err, arena);

    if (!jval) {
        printf("ERROR %d: first_bad_char:%d, line:%d col:%d start:%d end:%d  %s\n",
           err.err_type, err.first_bad_char,  err.line, err.column, err.parse_start, err.parse_end, err.message);
    }
    else {
        jsonp_print(jval, JSON_FORMAT_FLAGS_DEFAULT);
        printf("\n");
    }

    alok_arena_destroy(arena);
    jsonp_destroy();
}

static void parse_test_str_custom_init(
        char const * str,
        jp_bitset_t config_flags,
        uint32_t max_depth,
        char const * whitespace_chars )
{

    if (! whitespace_chars ) whitespace_chars = JSON_WHITESPACE_CHARS_DEFAULT;
    Error err = jsonp_init( config_flags, max_depth,whitespace_chars );
    if (err.err) {
        printf("reported error: %d, message: %s\n", err.reported_err, err.msg);
    }


    ArenaErrResult aer = alok_arena_create( ONE_MEBIBYTE * 100);
    if ( aer.err ) {
        printf("alok_arena_create failed with %d, %s\n", aer.reported_err, aer.msg);
    }
    AlokArena *arena = aer.result;

    JsonParseError json_err = {};
    printf("\nParsing json string '%s': \n", str);
    JsonValue *jval = jsonp_parse_string(str, &json_err, arena);
    if (!jval) {
        printf("ERROR %d: first_bad_char:%d, line:%d col:%d start:%d end:%d  %s\n",
           json_err.err_type, json_err.first_bad_char,  json_err.line, json_err.column, json_err.parse_start,
           json_err.parse_end, json_err.message);
    }
    else {
        jsonp_print(jval, JSON_FORMAT_FLAGS_DEFAULT);

        printf("\n");
    }

    alok_arena_destroy(arena);
    jsonp_destroy();

}


static void simple_parse(char const *json_text) {

    Error init_err = jsonp_init();
    if (init_err.err) {
        err_print(init_err);
        jsonp_destroy();
        return;
    }

    ArenaErrResult aer = alok_arena_create( 1024 * 124);  // initially 1MB as an example. Grows as needed.
    if ( aer.err ) {
        printf("alok_arena_create failed with %d, %s\n", aer.reported_err, aer.msg);
        jsonp_destroy();
        return;
    }
    AlokArena *arena = aer.result;

    JsonParseError err = {};

    JsonValue *jval = jsonp_parse_string(json_text, &err, arena);
    if (!jval) {
        // handle error
        jsonp_print_parse_error(&err);
    } else {
        jsonp_print(jval, JSON_FORMAT_FLAGS_DEFAULT);
        putchar('\n');
    }

    alok_arena_destroy(arena);
    jsonp_destroy();
}

static void test_custom_flags() {
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


static void test_multi_byte_char_strings() {
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

static void test_parse_unicode_escapes() {
    // parse_test_str("\"\\uCAFE \\uBABE\"");
    // parse_test_str("\"\\uCAFE\\uBABE\"");

    // parse_test_str("\"\\uD801\\uDC01\"");   // valid high- / low-surrogate pair=
    //
    // parse_test_str("\"\\uDC01\"");      //low surrogate without preceding high surrogate
    // parse_test_str("\"\\uD801\"");      //high surrogate without following low surrogate
    //
    // parse_test_str("\"\\uDC01\\uDC02\"");      //two low surrogates in a row
    // parse_test_str("\"\\uD801\\uD802\"");      //two high surrogates in a row


    // parse_test_str("\"\\u0041\""); // A
    // parse_test_str("\"\\u0080\"");  // ''
    //
    // parse_test_str("\"\\u0800\"");  // 'ࠀ'
    //
    // parse_test_str("\"\\uD834\\uDD1E\"");
    //
    // parse_test_str("\"😀  \\uD83D\\uDE00\"");
    //
    // parse_test_str("\"😀  \\U01F600\"");  // Our custom enhancement! Allows full unicode codepoint without surrogates


    // the code that renders glyphs combines them into one glyph!!
    // parse_test_str("\" combining character: C with tail: \\u0043\\u0327\"");

    // Test: Combining character vs Precomposed
    // \u00E9 is 'é' (1 codepoint)
    // e\u0301 is 'e' + '´' (2 codepoints)
    // printf("\n--- Combining Character Test ---\n");
    // parse_test_str("\"\\u00E9\"");
    // parse_test_str("\"e\\u0301\"");
    // printf("Note: Both should look identical in the terminal, but 'raw bytes' count will differ.\n");

    // parse_test_str("\"\\uABCDAPPLE\"");

    simple_parse("[\"\\uD888\\u1234\"]");
}

static void test_null_parse() {
    parse_test_str("null");
    // parse_test_str(" null ");
    // parse_test_str("nul");
    // parse_test_str("nu");
    // parse_test_str("n");
    //
    // parse_test_str("number");
    // parse_test_str("next");
    //
    // parse_test_str("nulll");
}

static void test_true_parse() {
    parse_test_str("true");
    parse_test_str(" true ");
    parse_test_str(" truetrue ");
    parse_test_str(" true true");
    parse_test_str(" tr true");
}

static void test_false_parse() {
    parse_test_str("false");
    parse_test_str(" false ");
    parse_test_str(" falsefalse ");
    parse_test_str(" false false");
    parse_test_str(" fals ");
}

static void test_number_parse() {
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

static void test_array_parse() {
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
    // parse_test_str("[[1,2 ] ] ");
    // parse_test_str("[[1 ], [2] ] ");
    // parse_test_str("[[1,2] ] ");
    // parse_test_str("[[1,2],[3,4]] ");
    // parse_test_str("[ [1,2],[3,4]] ");
    // parse_test_str("[ [1,2], [3,4]] ");
    // parse_test_str("[ [1,2], [3,4] ] ");

    // parse_test_str("[ [1,2,3], [4,5,6], [true,false,null] ] ");

    // simple_parse("[\"\",]");
    // simple_parse("[\"\",");
    // simple_parse("[\"\"");

    // 80 chars with error in middle to test error display
    // 85 chars
    simple_parse("[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16, 17,18,19   20,21,22,23,24,25,26,27,28,29,30,31]");
}

static void test_parse_objects() {
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

static void test_hex_and_chars() {
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

static void test_indeterminates() {
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

static void test_json_test_suite_fails() {
    simple_parse("{]");
    simple_parse(" "); // single space
    simple_parse(""); // no data
    simple_parse("[\"\\uD800\\uD800\n\"]"); //incomplete surrogate escape
    simple_parse("[\"\\uD888\\u1234\"])");  //1st surrogate valid, second invalid
    simple_parse("{\"\\uDFAA\":0}");

}

static void test_fails_for_reporting() {

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

static void parse_json_file(char const *filename) {
    Error init_err = jsonp_init();
    if (init_err.err) {
        err_print(init_err);
        jsonp_destroy();
        return;
    }
    ArenaErrResult aer = alok_arena_create( ONE_MEBIBYTE * 100);
    if ( aer.err ) {
        printf("alok_arena_create failed with %d, %s\n", aer.reported_err, aer.msg);
        jsonp_destroy();
        return;
    }
    AlokArena *arena = aer.result;

    JsonParseError err = {};
    printf("\nParsing json file '%s': \n", filename);
    // JsonValue *jval = jsonp_parse(str, &err, &arena);
    JsonValue *jval = jsonp_parse_file(filename, &err, arena);

    if (!jval) {
        // printf("ERROR %d: first_bad_char:%d, line:%d col:%d start:%d end:%d  %s\n",
        //    err.err_type, err.first_bad_char,  err.line, err.column, err.parse_start, err.parse_end, err.message);
        jsonp_print_parse_error(&err);
    }
    else {
        int chars_printed = 0;

        JsonFormatFlags flags = { .single_line = true, .indent = 2 };
        printf("\nPretty Printer:\n");
        printf("-----------------\n");
        printf("\nsingle line:\n");

        chars_printed = jsonp_print(jval, flags);
        printf("\n");
        printf("\ntotal_chars_printed= %d\n", chars_printed);

        printf("\nsprint to StringBuilder:\n");
        // todo (rob) StringBuilder init should take an Arena/allocator
        StringBuilder sb = {};
        sb_init(&sb, 64, "");
        jsonp_sprint(&sb, jval, flags);
        sb_print(&sb); // print the StringBuilder.

        sb_destroy(&sb);

        printf("\nmulti line:\n");
        flags.single_line = false;
        chars_printed = jsonp_print(jval, flags);
        printf("\n");
        printf("\ntotal_chars_printed= %d\n", chars_printed);
        printf("\nsprint to StringBuilder:\n");
        sb_init(&sb, 64, "");
        jsonp_sprint(&sb, jval, flags);
        sb_print(&sb); // print the StringBuilder.

        sb_destroy(&sb);

    }

    alok_arena_destroy(arena);
    jsonp_destroy();
}

static void parse_json_stream(char const *filename) {
    Error init_err = jsonp_init();
    if (init_err.err) {
        err_print(init_err);
        jsonp_destroy();
        return;
    }
    ArenaErrResult aer = alok_arena_create( ONE_MEBIBYTE * 100);
    if ( aer.err ) {
        printf("alok_arena_create failed with %d, %s\n", aer.reported_err, aer.msg);
        jsonp_destroy();
        return;
    }
    AlokArena *arena = aer.result;

    // FILE* fptr = nullptr;
    JsonValue *jval = nullptr;
    JsonParseError err = {};

    USING_FILE( fptr, filename, "rb") {
        int saved_errno = errno;

        if (!fptr) {
            // handle error
            // If stat() succeeded but fopen() failed, it's likely a different issue (e.g., too many open files, file is a directory)
            printf("Failed to open file '%s': %s", filename, strerror(saved_errno) );

        }

        printf("\nParsing json file as stream: '%s': \n", filename);
        jval = jsonp_parse_stream(fptr, &err, arena);
    }

    // FILE *fp = fopen(filename, "rb");
    // int saved_errno = errno;
    //
    // if (!fp) {
    //     // handle error
    //     // If stat() succeeded but fopen() failed, it's likely a different issue (e.g., too many open files, file is a directory)
    //     printf("Failed to open file '%s': %s", filename, strerror(saved_errno) );
    //
    // }
    // JsonParseError err = {};
    // printf("\nParsing json file as stream: '%s': \n", filename);
    // JsonValue *jval = jsonp_parse_stream(fp, &err, &arena);
    // fclose(fp);


    if (!jval) {
        // printf("ERROR %d: first_bad_char:%d, line:%d col:%d start:%d end:%d  %s\n",
        //    err.err_type, err.first_bad_char,  err.line, err.column, err.parse_start, err.parse_end, err.message);
        jsonp_print_parse_error(&err);
    }
    else {
        jsonp_print(jval, JSON_FORMAT_FLAGS_DEFAULT);
        printf("\n");
    }

    alok_arena_destroy(arena);
    jsonp_destroy();
}

static void parse_json_url(char const *url_string) {
    Error init_err = jsonp_init();
    if (init_err.err) {
        err_print(init_err);
        jsonp_destroy();
        return;
    }
    ArenaErrResult aer = alok_arena_create( ONE_MEBIBYTE * 100);
    if ( aer.err ) {
        printf("alok_arena_create failed with %d, %s\n", aer.reported_err, aer.msg);
        jsonp_destroy();
        return;
    }
    AlokArena *arena = aer.result;

    JsonParseError err = {};
    printf("\nParsing json file from url: '%s': \n", url_string);
    JsonValue *jval = jsonp_parse_url(url_string, &err, arena);


    if (!jval) {
        // printf("ERROR %d: first_bad_char:%d, line:%d col:%d start:%d end:%d  %s\n",
        //    err.err_type, err.first_bad_char,  err.line, err.column, err.parse_start, err.parse_end, err.message);
        jsonp_print_parse_error(&err);
    }
    else {
        printf("\n");
        jsonp_print(jval, JSON_FORMAT_FLAGS_DEFAULT);
    }

    alok_arena_destroy(arena);
    jsonp_destroy();
}

static void test_one_json_file() {
    // parse_json_file(nullptr);
    // parse_json_file("");
    // parse_json_file("no such file");
    //
    // parse_json_file("../test/json_parser/JSONTestSuite/pass/y_string_1_2_3_bytes_UTF-8_sequences.json");
    // parse_json_file("../test/json_parser/JSONTestSuite/pass/y_array_with_several_null.json");
    //
    // //fail
    // parse_json_file("../test/json_parser/JSONTestSuite/fail/i_structure_500_nested_arrays.json");

    // parse_json_file("../test/json_parser/JSONTestSuite/fail/n_object_with_single_string.json");

    // n_structure_single_eacute.json
    // parse_json_file("../test/json_parser/JSONTestSuite/fail/n_structure_single_eacute.json");

    // this has only a BOM. It recocnizes it as empty text, but uses the length of the bom in the error message incorrectly
    // n_structure_UTF8_BOM_no_data.json
    // parse_json_file("../test/json_parser/JSONTestSuite/fail/n_structure_UTF8_BOM_no_data.json");

    // n_multidigit_number_then_00.json
    // want fail
    // parse_json_file("../test/json_parser/JSONTestSuite/fail/n_multidigit_number_then_00.json");

    // when parsed as a file this gives the wrong error, 3:UNEXPECTED_TEXT  line:1 col:1 pos:1 start:1 end:1 :  unexpected character: EOF
    // It should give a 'only whitespace' error

    // n_single_space.json
    // parse_json_file("../test/json_parser/JSONTestSuite/fail/n_single_space.json");


    // parse_json_file("../test/json_parser/JSONTestSuite/fail/n_string_1_surrogate_then_escape_u1.json");
    // simple_parse("[1,2 3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31]");
    // parse_json_file("../test/json_parser/json_files/n_long_array_1.json");
    // parse_json_stream("../test/json_parser/json_files/n_long_array_1.json");

    // parse_json_file("../test/json_parser/json_files/todos.json");

    parse_json_file("../test/json_parser/json_files/RFC8259_example_13.1.json");
    putchar('\n');
    parse_json_file("../test/json_parser/json_files/RFC8259_example_13.2.json");




}

static void test_one_url() {
    parse_json_url("https://jsonplaceholder.typicode.com/todos");
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
    // test_number_parse();
    // test_array_parse();
    // test_parse_objects();
    test_parse_unicode_escapes();

    // test_multi_byte_char_strings();

    // test_hex_and_chars();

    // test_indeterminates();

    // test_custom_flags();

    // test_fails_for_reporting();
    // test_json_test_suite_fails();

    // test_one_json_file();

    // test_one_url();

}
#endif
