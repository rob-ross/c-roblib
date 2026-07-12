// file_utils.c
// Created by Rob Ross on 7/11/26.
//

#include "file_utils.h"

#include <sys/syslimits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>


// todo (rob) we need an ignore list for things like .DS_Store, flags for handling hidden files/directories, links, etc.
static void walk_directory_impl(const char *path, uint16_t depth ) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("Unable to open directory");
        return;
    }

    struct dirent *entry;
    struct stat file_stat;
    char full_path[PATH_MAX];

    while ((entry = readdir(dir)) != NULL) {
        // Skip the current and parent directory pointers
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct the full path for the stat call
        int len = snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (len >= (int)sizeof(full_path)) {
            fprintf(stderr, "Path too long: %s/%s\n", path, entry->d_name);
            continue;
        }
        // Use lstat to avoid following symlinks and correctly identify them
        if (lstat(full_path, &file_stat) == -1) {
            continue;
        }

        // Print indentation
        for (int i = 0; i < depth; i++) printf("  ");

        /*
            S_ISDIR(m): Returns non-zero if the path is a directory.
            S_ISREG(m): Returns non-zero if it is a "regular" file (not a pipe, socket, or device).
            S_ISLNK(m): Returns non-zero if it is a symbolic link (requires lstat instead of stat)
         */
        const bool is_dir = S_ISDIR(file_stat.st_mode);
        const bool is_link = S_ISLNK(file_stat.st_mode);
        const bool is_file = S_ISREG(file_stat.st_mode);


        if (is_dir) {
            // Directory
            printf("[DIR]  %s\n", entry->d_name);
            walk_directory_impl(full_path, depth + 1);
        } else if (is_link || is_file) {
            char const * type = is_link ? "[LINK]" : "[FILE]";
            const char *ext = strrchr(entry->d_name, '.');
            if (!ext || ext == entry->d_name) {
                ext = "none";
            } else {
                ext++; // Move past the dot
            }

            printf("%s %-20s | Ext: %-5s | Size: %lld bytes\n",
                type,
                entry->d_name,
                ext,
                (long long)file_stat.st_size);
        }
    }

    closedir(dir);
}

void walk_directory(const char *path) {
    walk_directory_impl(path, 0);
}

int main(void) {
    walk_directory("../");
    return 0;
}
