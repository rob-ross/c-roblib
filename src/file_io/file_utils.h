// file_utils.h
// Created by Rob Ross on 7/11/26.
//

#ifndef FILE_UTILS_H
#define FILE_UTILS_H
#include <stddef.h>

// -----------------------------------------------------------------
//      Writer Interface
//
//  Abstracts writing of data to generic output sources, such as
//     stdout, a stream, a char buffer, etc.
// -----------------------------------------------------------------

typedef struct Writer Writer;
struct Writer {
    void (*write)(Writer *self, const char *data_source, size_t len);
    void *writer_context;  // E.g., for a FileWriter this is a FILE*, for a BufferedWriter it's a char[] buffer
};


/**
 * Recursively walks a directory path and prints information.
 * @param path The starting directory path.
 */
void walk_directory( const char *path );

#endif // FILE_UTILS_H
