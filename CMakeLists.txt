cmake_minimum_required(VERSION 4.2)
project(c_roblib C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set a default build type if none is specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Helpful for debugging (CLion usually does this in Debug, but explicit is fine)
# -fsanitize=address -fsanitize=leak
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g -fno-omit-frame-pointer -fsanitize=address -fsanitize=leak \
-Wall -Werror -Wno-unknown-attributes -Wno-unused-const-variable")

# Use add_compile_options with generator expressions to robustly add release flags
# This ensures they are applied even if the cache has old values.
add_compile_options($<$<CONFIG:Release>:-O3>)
add_compile_options($<$<CONFIG:Release>:-march=native>)
add_compile_options($<$<CONFIG:Release>:-DNDEBUG>)

add_library(c_roblib STATIC library.c)



add_executable( json_parse
        src/json_parser/json_parser.c
        src/json_parser/arena.c
        src/json_parser/error_result.c
        src/string/string_utils.c
)
target_compile_options(json_parse PRIVATE -Wno-unused-function)