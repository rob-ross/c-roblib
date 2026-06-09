//
// Created by Rob Ross on 2/4/26.
//
#pragma once
#include <stdint.h>
#include <string.h>
#include <wchar.h>

#if defined(__has_include) && __has_include(<uchar.h>)
#include <uchar.h>
#else
typedef uint16_t char16_t;
typedef uint32_t char32_t;
#endif

typedef enum string_buffer_type_e {
    SBTYPE_ASCII,
    SBTYPE_WIDE,
    SBTYPE_UTF16,
    SBTYPE_UTF32,
    SBTYPE_NULL,
} SBType;


typedef struct string_buffer_s {
    SBType type;
    size_t length;
    union {
        char      *as_char;  // Standard C strings (usually ASCII or UTF-8)
        wchar_t   *as_wide;  // Native platform wide strings (L"...") & System APIs
        char16_t *as_utf16;  // Fixed-width UTF-16 (good for Windows interop/serialization)
        char32_t *as_utf32;  // Fixed-width UTF-32 (good for decoding/algorithms)
    } buffer;
} StringBuffer;

// Null object pattern
extern const StringBuffer NULL_STRING_BUFFER;

static const size_t SB_MAX_ARGS = 1024;  // max number of variadic arguments processed


/**
 *
 * Return a new StringBuffer with argument string centered in a string of length width.
 * Padding is done using the specified fill_char.
 * The argument StringBuffer is returned if width is less than or equal to len(s).
 */
StringBuffer * sb_centered(const StringBuffer *sb, int width, char fill_char);

/*
 * Makes a new copy of the argument StringBuffer.
 */
StringBuffer * sb_copy(const StringBuffer *sb);

void sb_destroy_string_buffer(StringBuffer *sb);
void sb_display_StringBuffer(const StringBuffer *sb);
StringBuffer * sb_join(const char *separator, StringBuffer *sb1, ...);
// Returns mutable pointer. Returns NULL on allocation failure, not NULL_STRING_BUFFER.
StringBuffer * sb_new_string_buffer_from_string(const char *str);
char * sb_string_buffer_repr(const StringBuffer *sb);
const char * sb_type_name(SBType type);
StringBuffer * sb_zfill(StringBuffer *sb, int width);