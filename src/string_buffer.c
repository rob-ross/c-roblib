//
// Created by Rob Ross on 2/4/26.
//
// String methods that work on an ASCII string buffer.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "string_buffer.h"

#include "string_utils.h"

const StringBuffer NULL_STRING_BUFFER = {.type = SBTYPE_NULL, .length = 0, .buffer.as_char = NULL};


void sb_destroy_string_buffer(StringBuffer *sb) {
    if (sb == &NULL_STRING_BUFFER){
        return; // never try to free our Null object!
    }

    if (sb) {
        if (sb->buffer.as_char){
            free(sb->buffer.as_char);
        }
        sb->buffer.as_char = NULL;
        sb->length = 0;
        sb->type = SBTYPE_NULL;
        free(sb);
    }
}


StringBuffer * sb_centered(const StringBuffer *sb, const int width, const char fill_char) {
    if (!sb) return NULL;
    if (width <=0 || width <= sb->length) {
        return sb_copy(sb);
    }
    char *temp_string = sutil_pad_center(sb->buffer.as_char, width, fill_char);
    StringBuffer *new_sb = sb_new_string_buffer_from_string(temp_string);
    free(temp_string);
    return new_sb;
}

StringBuffer * sb_copy(const StringBuffer *sb) {
    if (!sb) return NULL;
    return sb_new_string_buffer_from_string(sb->buffer.as_char);
}


void sb_display_StringBuffer(const StringBuffer *sb){
    switch( sb->type) {
        case SBTYPE_ASCII:
            printf("StringBuffer(type=%s, length=%zu, char * buffer='%s')", sb_type_name(sb->type), sb->length, sb->buffer.as_char);
            break;

        default:
            printf("StringBuffer(type=unknown, length=%zu, buffer address=%p)",  sb->length, &sb->buffer);
            break;
    }
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnusedValue"
/**
 *
 *

Return a newly malloc'd StringBuffer which is the concatenation of the StringBuffers in the argument list.
The separator between elements is the string passed in `separator`. The last argument must be NULL.
*/
StringBuffer * sb_join(const char *separator, StringBuffer *sb1, ...){
    if (!separator || !sb1){
        return NULL;
    }

    const size_t separator_length = strlen(separator);

    va_list args;
    va_start(args, sb1);
    const StringBuffer *next;
    // we need a Vector<StringBuffer> here to collect the args..... use a fixed buffer for now.
    const StringBuffer *arg_list[SB_MAX_ARGS];
    size_t arg_list_count = 0;

    arg_list[arg_list_count++] = sb1;  // add first non-variadic arg to array

    size_t total_length = sb1->length;
    // Loop until NULL sentinel is found or max arguments is reached
    while ( arg_list_count < SB_MAX_ARGS && (next = va_arg(args, const StringBuffer *)) != NULL) {
        arg_list[arg_list_count++] = next;
        total_length += next->length;
    }
    va_end(args);

    total_length += (arg_list_count - 1) * separator_length;  // separators go between strings

    // we need to analyze the incomming StringBuffers types. The longest type controls the output type. I.e., if
    // one is ASCII and the other is wchar_t we need to promote the ascii chars to wide types, etc.
    // this current simple implementation just assumes all arguments are type ascii.
    switch( arg_list[0]->type) {
        case SBTYPE_ASCII: {
            char *new_string = malloc(sizeof(char) * total_length + 1);
            if (!new_string){
                return NULL;
            }
            int loop_index = 0;
            size_t chars_written = 0;
            for ( ; loop_index < arg_list_count - 1; ++loop_index) {
                memcpy(&new_string[chars_written], arg_list[loop_index]->buffer.as_char, arg_list[loop_index]->length);
                chars_written += arg_list[loop_index]->length;
                memcpy(&new_string[chars_written], separator, separator_length);
                chars_written += separator_length;
            }
            memcpy(&new_string[chars_written], arg_list[loop_index]->buffer.as_char, arg_list[loop_index]->length); // copy last string without separator
            chars_written += arg_list[loop_index]->length;

            new_string[total_length] = '\0';  // terminate string (index is 0 to length-1, so length is the null slot)

            StringBuffer *new_string_buffer = sb_new_string_buffer_from_string(new_string);
            
            free(new_string); // Free the temporary buffer, as the constructor made its own copy
            return new_string_buffer;
        }

        default:
            printf("join() for non-ascii StringBuffer is not supported. \n");
            break;
    }
    return NULL;
}
#pragma clang diagnostic pop



/**
 * Creates a new StringBuffer object on the heap and makes a copy of the str argument.
 * Call sb_destroy_string_buffer to reclaim StringBuffer and internal storage memory
 * @param str
 * @return
 */
StringBuffer *sb_new_string_buffer_from_string(const char *str) {
    const size_t str_len = strlen(str);
    char *str_copy = malloc(str_len + 1);
    if (!str_copy) {
        return NULL;
    }
    StringBuffer *new_string_buffer = malloc(sizeof(StringBuffer));
    if (!new_string_buffer){
        free(str_copy);
        return NULL;
    }
    strcpy(str_copy, str);
    new_string_buffer->length = strlen(str_copy);
    new_string_buffer->type = SBTYPE_ASCII;
    new_string_buffer->buffer.as_char = str_copy;
    return new_string_buffer;
}

char *sb_string_buffer_repr(const StringBuffer *sb) {
    if (!sb) return NULL;
    const char *type_name = sb_type_name(sb->type);
    int num_bytes = 0;
    char * repr_string = NULL;

    switch( sb->type) {
        case SBTYPE_ASCII: {
            const char *format_string = "StringBuffer(type=%s, length=%zu, buffer='%s')";
            num_bytes = snprintf(NULL, 0, format_string, type_name, sb->length, sb->buffer.as_char);
            if (num_bytes < 0) return NULL; // Encoding error
            repr_string = malloc(sizeof(char) * num_bytes + 1);
            if (!repr_string)  return NULL;
            snprintf(repr_string, num_bytes + 1, format_string, type_name, sb->length, sb->buffer.as_char);
            return repr_string;
        }
        default: {
            const char *format_string = "StringBuffer(type=unknown, length=%zu, buffer address=%p)";
            num_bytes = snprintf(NULL, 0, format_string, sb->length, (void *) &sb->buffer);
            if (num_bytes < 0) return NULL; // Encoding error
            repr_string = malloc(sizeof(char) * num_bytes + 1);
            if (!repr_string) return NULL;
            snprintf(repr_string, num_bytes + 1, format_string, sb->length, (void *) &sb->buffer);
            return repr_string;
        }
    }
}

const char * sb_type_name(const SBType type) {
    switch (type) {
        case SBTYPE_ASCII:
            return "ascii";
        case SBTYPE_WIDE:
            return "wide";
        case SBTYPE_UTF16:
            return "utf16";
        case SBTYPE_UTF32:
            return "utf32";
        case SBTYPE_NULL:
            return "null";
        default:
            return "unknown";
    }
}


/*
 *  Return the argument StringBuffer with a widened-reallocated char buffer that is left filled with ASCII '0' digits
 *  to make a string of length width.
 *  A leading sign prefix ('+'/'-') is handled by inserting the padding after the sign character
 *  rather than before. The original string is unchanged if width is less than or equal to len(s), or if realloc fails.
 */
StringBuffer * sb_zfill(StringBuffer *sb, int width) {
    if (width <= sb->length) {
        return sb;
    }

    switch (sb->type) {
        case SBTYPE_ASCII: {
            // 1. Reallocate memory to fit the new width
            char *new_ptr = realloc(sb->buffer.as_char, (size_t)width + 1);
            if (!new_ptr) {
                // Handle allocation failure (keep old buffer)
                return sb;  // I hate the silent failures in C. Need to come up with a better error reporting strategy
            }
            sb->buffer.as_char = new_ptr;

            // 2. Determine offset for sign handling
            size_t padding = width - sb->length;
            size_t data_start = 0;
            if (sb->buffer.as_char[0] == '+' || sb->buffer.as_char[0] == '-') {
                data_start = 1;
            }

            // 3. Shift the number part to the right (memmove handles overlap)
            memmove(sb->buffer.as_char + data_start + padding, sb->buffer.as_char + data_start, sb->length - data_start + 1);

            // 4. Fill the gap with zeros
            memset(sb->buffer.as_char + data_start, '0', padding);
            sb->length = width;
            break;

        }
        default: {
            printf("sb_zfill not implemented for type %s\n", sb_type_name(sb->type));
        }
    }
    return sb;
}


void t_zfill(void){
    char *temp = "+42";
    StringBuffer *sb = sb_new_string_buffer_from_string(temp);
    printf("buffer string: %s\n", sb->buffer.as_char);
    if ( !sb || sb->type == SBTYPE_NULL) {
        // No need to destroy, as creation failed and returned the null object.
        exit(1);
    }
    sb_zfill(sb, 10);
    printf("sb zero-filled to 10 is: %s\n", sb->buffer.as_char);

    sb_destroy_string_buffer(sb); // Clean up the allocated memory
}

// make: clang string_buffer.c string_util.c -o string_buffer.out


int main(void) {
    StringBuffer *sb1 = sb_new_string_buffer_from_string("Foo");
    StringBuffer *sb2 = sb_new_string_buffer_from_string("Bar");
    StringBuffer *sb3 = sb_new_string_buffer_from_string("Baz");

    StringBuffer *result = sb_join(", ", sb1, sb2, sb3, NULL);

    printf("result = %s\n", result->buffer.as_char);

    char *ptr = NULL;
    printf("repr: %s\n", (ptr = sb_string_buffer_repr(sb3) ) ) ;
    free(ptr);

    StringBuffer *centered= sb_centered(sb3, 100, '#');
    sb_display_StringBuffer(centered);
    printf("\n");
    sb_destroy_string_buffer(centered);


    sb_destroy_string_buffer(sb1); // Clean up the allocated memory
    sb_destroy_string_buffer(sb2); // Clean up the allocated memory
    sb_destroy_string_buffer(sb3); // Clean up the allocated memory

}