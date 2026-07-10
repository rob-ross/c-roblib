// unicode_tools.c
// Created by Rob Ross on 7/9/26.
//

#include "../../include/roblib/unicode_tools.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

// Concrete implementation for FILE streams (stdout, stderr, files)
void file_writer_impl(Writer *self, const char *data, size_t len) {
    fwrite(data, 1, len, (FILE *)self->ctx);
}

Writer writer_to_file(FILE *f) {
    return (Writer){.write = file_writer_impl, .ctx = f};
}
// Concrete implementation for memory buffers
void buffer_writer_impl(Writer *self, const char *data, size_t len) {
    BufferCtx *ctx = self->ctx;
    size_t space_left = (ctx->used < ctx->capacity) ? (ctx->capacity - ctx->used) : 0;
    size_t to_copy = (len < space_left) ? len : space_left;
    if (to_copy > 0) {
        memcpy(ctx->buf + ctx->used, data, to_copy);
        ctx->used += to_copy;
    }
}

// Helper for formatted output via the Writer interface
void writer_printf(Writer *w, const char *fmt, ...) {
    char temp[256]; // Temporary stack buffer for formatting
    va_list args = {};
    va_start(args, fmt);
    int n = vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);
    if (n > 0) {
        w->write(w, temp, (size_t)n >= sizeof(temp) ? sizeof(temp) - 1 : (size_t)n);
    }
}

// Helper to decode a single UTF-8 codepoint and return the number of bytes consumed
static int utf8_to_codepoint(const char *utf8, uint32_t *out_cp) {
    unsigned char *s = (unsigned char *)utf8;
    if (s[0] < 0x80) { *out_cp = s[0]; return 1; }
    if ((s[0] & 0xE0) == 0xC0) { *out_cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F); return 2; }
    if ((s[0] & 0xF0) == 0xE0) { *out_cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); return 3; }
    if ((s[0] & 0xF8) == 0xF0) { *out_cp = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F); return 4; }
    return 1; // Error fallback
}

// copy diagnostic information for the codepoint argument into the Writer
void repr_hex_and_chars_for_codepoint(Writer *w, uint32_t codepoint) {
    char utf8[5] = {0};
    if (codepoint <= 0x7F) {
        utf8[0] = (char)codepoint;
    } else if (codepoint <= 0x7FF) {
        utf8[0] = (char)(0xC0 | (codepoint >> 6));
        utf8[1] = (char)(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        utf8[0] = (char)(0xE0 | (codepoint >> 12));
        utf8[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        utf8[2] = (char)(0x80 | (codepoint & 0x3F));
    } else {
        utf8[0] = (char)(0xF0 | (codepoint >> 18));
        utf8[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        utf8[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        utf8[3] = (char)(0x80 | (codepoint & 0x3F));
    }

    uint32_t hex_val = 0;
    for(int i = 0; utf8[i]; i++) {
        hex_val = (hex_val << 8) | (unsigned char)utf8[i];
    }

    writer_printf(w, "CP: %-7u | Hex: 0x%-8X | Char: %s | Esc: ",
                  codepoint, hex_val, utf8);
    if (codepoint <= 0xFFFF) writer_printf(w, "\\u%04X", codepoint);
    else writer_printf(w, "U+%06X", codepoint);
    w->write(w, "\n", 1);
}

// copy diagnostic information about the chars in the string argument into the Writer.
/**
    Copy diagnostic information about the chars in the string argument into the Writer.
    Writes a repr str of four lines,
    First line is base 10 int representing Unicode code point of each char in str, i.e. ord(str[n])
    second line is hex values of each character in str as UTF-8 encoded bytes,
    third line is each original character in string,
    fourth line is Unicode escape sequence for each character
    Example:
    if string == "Apôñéas\u253C\U0001F604\U0001F64F", output is:
      65    112    244    241    233     97    115     9532     128516     128591
     0x41   0x70  0xC3B4 0xC3B1 0xC3A9  0x61   0x73  0xE294BC 0xF09F9884 0xF09F998F
      A      p      ô      ñ      é      a      s       ┼         😄          🙏
    \u0041 \u0070 \u00f4 \u00f1 \u00e9 \u0061 \u0073  \u253c  \U0001f604 \U0001f64f
 * @param w
 * @param str
 */
void repr_hex_and_chars(Writer *w, char const *str) {
    if (!str) return;

    #define MAX_REPR_COLS 128
    uint32_t cps[MAX_REPR_COLS];
    char utf8_parts[MAX_REPR_COLS][5];
    int col_widths[MAX_REPR_COLS];
    int count = 0;

    const char *ptr = str;
    while (*ptr && count < MAX_REPR_COLS) {
        uint32_t cp;
        int bytes = utf8_to_codepoint(ptr, &cp);
        cps[count] = cp;

        // Capture the raw UTF-8 bytes for this character
        memset(utf8_parts[count], 0, 5);
        memcpy(utf8_parts[count], ptr, bytes);

        // Calculate column width (max of dec, hex, and escape string lengths)
        char tmp[32];
        int w1 = snprintf(tmp, sizeof(tmp), "%u", cp);

        uint32_t hex_bytes = 0;
        for(int i=0; i<bytes; i++) hex_bytes = (hex_bytes << 8) | (unsigned char)ptr[i];
        int w2 = snprintf(tmp, sizeof(tmp), "0x%X", hex_bytes);

        int w3 = (cp <= 0xFFFF) ? 6 : 10; // \uXXXX vs \UXXXXXXXX

        int max = w1 > w2 ? w1 : w2;
        col_widths[count] = (max > w3 ? max : w3) + 2; // +2 for spacing

        ptr += bytes;
        count++;
    }

    // Line 1: Codepoints (Decimal)
    for (int i = 0; i < count; i++) writer_printf(w, "%-*u", col_widths[i], cps[i]);
    w->write(w, "\n", 1);

    // Line 2: Hex UTF-8 bytes
    for (int i = 0; i < count; i++) {
        uint32_t hex_bytes = 0;
        int bytes = (int)strlen(utf8_parts[i]);
        for(int j=0; j<bytes; j++) hex_bytes = (hex_bytes << 8) | (unsigned char)utf8_parts[i][j];
        writer_printf(w, "0x%-*X", col_widths[i] - 2, hex_bytes);
    }
    w->write(w, "\n", 1);

    // Line 3: Actual Characters
    for (int i = 0; i < count; i++) {
        w->write(w, utf8_parts[i], strlen(utf8_parts[i]));
        for (int j = 0; j < col_widths[i] - 1; j++) w->write(w, " ", 1); // Approximation
    }
    w->write(w, "\n", 1);

    // Line 4: Escapes
    for (int i = 0; i < count; i++) {
        if (cps[i] <= 0xFFFF)
            writer_printf(w, "\\u%04X%-*s", cps[i], col_widths[i]-6, "");
        else
            writer_printf(w, "\\U%06X%-*s", cps[i], col_widths[i]-8, "");
    }
    w->write(w, "\n", 1);
}

