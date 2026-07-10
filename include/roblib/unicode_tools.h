//  unicode_tools.h
//  Created by Rob Ross on 7/9/26.
//

#pragma once
#ifndef C_ROBLIB_UNICODE_TOOLS_H
#define C_ROBLIB_UNICODE_TOOLS_H

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

// -----------------------------------------------------------------
//      Writer Interface
// -----------------------------------------------------------------

typedef struct Writer Writer;
struct Writer {
    void (*write)(Writer *self, const char *data, size_t len);
    void *ctx;
};


typedef struct {
    char *buf;
    size_t capacity;
    size_t used;
} BufferCtx;

void file_writer_impl(Writer *self, const char *data, size_t len);
Writer writer_to_file(FILE *f);
void buffer_writer_impl(Writer *self, const char *data, size_t len);


/**
    Examples of usage:
    char const * str = "Apôñéas\u253C\U0001F604\U0001F64F";
    // Output to Console
    printf("Console Output:\n");
    Writer out = writer_to_file(stdout);
    repr_hex_and_chars(&out, str);
    repr_hex_and_chars_for_codepoint(&out, 0x01F64F);

    Console Output:
    65      112     244     241     233     97      115     9532      128516      128591
    0x41    0x70    0xC3B4  0xC3B1  0xC3A9  0x61    0x73    0xE294BC  0xF09F9884  0xF09F998F
    A       p       ô       ñ       é       a       s       ┼         😄           🙏
    \u0041  \u0070  \u00F4  \u00F1  \u00E9  \u0061  \u0073  \u253C    \U01F604    \U01F64F
    CP: 128591  | Hex: 0xF09F998F | Char: 🙏 | Esc: U+01F64F

 */
void repr_hex_and_chars(Writer *w, char const *str);
void repr_hex_and_chars_for_codepoint(Writer *w, uint32_t codepoint);


#endif //C_ROBLIB_UNICODE_TOOLS_H
