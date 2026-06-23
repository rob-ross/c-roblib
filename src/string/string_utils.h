//
// Created by Rob Ross on 2/1/26.
//
// Various utility functions operating on ASCII strings.

#pragma once
#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif








static const size_t SUTIL_MAX_ARGS = 1024;  // max number of variadic arguments processed

static const char  * const SUTIL_WHITESPACE = " \t\n\r\v\f";

typedef enum case_e { CASE_INSENSITIVE, CASE_SENSITIVE } Case;

/*
const char const *whitespace = " \t\n\r\v\f";
ascii_lowercase = 'abcdefghijklmnopqrstuvwxyz'
ascii_uppercase = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
ascii_letters = ascii_lowercase + ascii_uppercase
digits = '0123456789'
hexdigits = digits + 'abcdef' + 'ABCDEF'
octdigits = '01234567'
punctuation = r"""!"#$%&'()*+,-./:;<=>?@[\]^_`{|}~"""
printable = digits + ascii_letters + punctuation + whitespace
*/


/**
 * Returns the character at the specified index as a Unicode code point.
 * @param str the non-NULL input string
 * @param index the zero-based index of the character to retrieve
 * @return the character at the specified index, or UINT32_MAX if str is NULL or index is out of bounds.
 */
uint32_t sutil_char_at(char const *str, size_t index);


/**
 * Returns the count of characters in the string. I.e., strlen(str). Although an ASCII string's character count
 * is the same as strlen(), for other encodings (like utf-8), char_count() may not == strlen().
 * @param str the non-NULL string argument.
 * @return the count of characters, i.e., strlen(str), or 0 if str is NULL.
 */
size_t sutil_char_count(char const *str);

/**
 * Returns true if the char `c` is found in the `chars` array.
 * @param c the character to check
 * @param chars the characters to check against, not NULL.
 * @return true if char `c` is found in `chars`, otherwise return false.
 */
bool sutil_char_in(uint32_t c, const char *chars);

/**
 * Concatenates the strings in the argument list and returns a newly allocated string. The last argument must be NULL.
 *
 * @param str1 first const char* to concat
 * @param ... const char* zero or more additional strings
 * @return a newly allocated string concatenation of the argument list. Caller is responsible for freeing the string.
 */
char * sutil_concat_strings(const char *str1, ...);
/**
 * Returns a newly allocated string that is a copy of the input string.
 * @param str input string, non-null
 * @return a newly allocated string copy of the original string, or nullptr if error occurred.
 */
char * sutil_copy_char(const char *str);


/**
 * Returns true if the input string ends with the specified suffix, otherwise return false.
 * @param str the input string, non-null
 * @param suffix the substring to match at the end of the input string, non-null
 * @return true if `str` ends with `suffix`, else false.
 */
bool sutil_ends_with(const char *str, const char *suffix);

#define sutil_index_SELECT(_1, _2, _3, _4, NAME, ...) NAME
// GCC and Clang (which defines __GNUC__) both support statement expressions
#if defined(__GNUC__)
// using gnu "statement expression" here instead of do-while because we need to return a value from the block
/**
 * sutil_index(const char *str, const char *substring, [size_t start [, size_t end ]]
 *
 * Returns the index of the first occurrence of `substring` in `str`, both non-null.
 * Optional arguments `start` and `end` allow specifying where to start/stop looking for `substring` in `str`.
 * Returns -1 if `substring` is not found.
 *
 */
#define sutil_index(...) \
    __extension__ ({ \
        extern int sutil_index_(const char *str, const char *substring); \
        extern int sutil_index_start_(const char *str, const char *substring, size_t start); \
        extern int sutil_index_start_end_(const char *str, const char *substring, size_t start, size_t end); \
        sutil_index_SELECT(__VA_ARGS__, sutil_index_start_end_, sutil_index_start_, sutil_index_)(__VA_ARGS__); \
    })
#else
#define sutil_index(...) \
    sutil_index_SELECT(__VA_ARGS__, sutil_index_start_end_, sutil_index_start_, sutil_index_)(__VA_ARGS__)
    // In non-GNU environments, these must be exposed to the file scope so the macro can see them.
    extern int sutil_index_(const char *str, const char *substring);
    extern int sutil_index_start_(const char *str, const char *substring, size_t start);
    extern int sutil_index_start_end_(const char *str, const char *substring, size_t start, size_t end);
#endif





/**
 * Returns a new copy of the string with all the cased characters converted to lowercase.
 * Operates by calling tolower() on each character of the input string.
 * @param str ASCII string
 * @return a new copy of the string with all the cased characters converted to lowercase.
 */
char * sutil_lower(const char *str) ;



/**
 * Returns a new string with argument `str` centered in a string of length width.
 * Padding is done using the specified fill_char.
 * A copy of the original string is returned if width is less than or equal to len(s).
 * @param str source string to center fill
 * @param width length of new padded string
 * @param fill_char the character to use for padding
 * @return newly allocated string.
 */
char * sutil_pad_center(const char* str, int width, char fill_char);

/**
 * Returns a newly allocated string that is size `width`, left-padded with the fill character.
 * If `width` is <= strlen(str), returns a new copy of the argument string (unchanged.).
 * @param str source string to left fill
 * @param width length of new padded string
 * @param fill_char the character to use for padding
 * @return newly allocated string.
 */
char * sutil_pad_left(const char *str, int width, char fill_char);

/**
 * Returns a newly allocated string that is size `width`, right-padded with the fill character.
 * If `width` is <= strlen(str), returns a new copy of the argument string (unchanged.).
 * @param str source string to right fill
 * @param width length of new padded string
 * @param fill_char the character to use for padding
 * @return newly allocated string.
 */
char * sutil_pad_right(const char *str,int width, char fill_char);


/**
 * Returns true if the input string starts with the specified prefix, otherwise return false.
 * @param str the input string
 * @param prefix the substring to match at the start of the input string
 * @return true if `str` starts with `prefix`, else false.
 */
bool sutil_starts_with(const char *str, const char *prefix);

bool sutil_strings_equal(const char *str1, const char *str2);
bool sutil_strings_equal_case(const char *str1, const char *str2, Case c);
bool sutil_strings_same(const char *s1, const char *s2);

/**
 * Returns a newly allocated copy of the string with leading and trailing characters in `chars` removed.
 * The chars argument is a string specifying the set of characters to be removed. If empty or NULL, the chars argument
 * defaults to removing whitespace as specified in SUTIL_WHITESPACE.
 * Example:
 *  char *str = "     five leading and trailing spaces removed     ";
 *  char *str_stripped = sutil_strip( str, "");
 *  str_stripped == "five leading and trailing spaces removed".
 *  sutil_strip( str, ""), sutil_strip( str, " "), and sutil_strip( str, NULL) are equivalent for this purpose.
 *
 * @param str input string to strip of leading and trailing chars
 * @param chars the array of chars to check. If empty or NULL, SUTIL_WHITESPACE is used as chars
 * @return a newly allocated copy of the string with leading and trailing characters removed.
 */
char * sutil_strip(const char *str, const char *chars);

/**
 * Returns a newly allocated copy of the string with leading characters in `chars` removed.
 * The chars argument is a string specifying the set of characters to be removed. If empty or NULL, the chars argument
 * defaults to removing whitespace as specified in SUTIL_WHITESPACE.
 * @param str input string to strip of leading chars
 * @param chars the array of chars to check. If empty or NULL, SUTIL_WHITESPACE is used as chars
 * @return a newly allocated copy of the string with leading characters removed.
 */
char * sutil_strip_left(const char *str, const char *chars);

/**
 * Returns a newly allocated copy of the string with trailing characters in `chars` removed.
 * The chars argument is a string specifying the set of characters to be removed. If empty or NULL, the chars argument
 * defaults to removing whitespace as specified in SUTIL_WHITESPACE.
 * @param str input string to strip of trailing chars
 * @param chars the array of chars to check. If empty or NULL, SUTIL_WHITESPACE is used as chars
 * @return a newly allocated copy of the string with trailing characters removed.
 */
char * sutil_strip_right(const char *str, const char *chars);

/**
 * Returns a new copy of the string with all the cased characters converted to uppercase.
 * Operates by calling toupper() on each character of the input string.
 * @param str ASCII string
 * @return a new copy of the string with all the cased characters converted to uppercase.
 */
char * sutil_upper(const char *str);

char * sutil_zfill(const char* str, int width);

#ifdef __cplusplus
}
#endif
