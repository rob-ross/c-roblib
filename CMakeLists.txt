cmake_minimum_required(VERSION 4.2)
project(c_roblib C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Set a default build type if none is specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Helpful for debugging (CLion usually does this in Debug, but explicit is fine)
# -fsanitize=address -fsanitize=leak
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g -fno-omit-frame-pointer -fsanitize=address -fsanitize=leak \
-Wall -Wpedantic -Wextra -Werror -Wno-unknown-attributes -Wno-unused-const-variable")

# Use add_compile_options with generator expressions to robustly add release flags
# This ensures they are applied even if the cache has old values.
add_compile_options($<$<CONFIG:Release>:-O3>)
add_compile_options($<$<CONFIG:Release>:-march=native>)
add_compile_options($<$<CONFIG:Release>:-DNDEBUG>)

# Common library containing shared code. 
# Adding files here ensures CLion correctly identifies them as part of the project
# and applies the C23 standard (solving "cannot resolve symbol 'bool'" errors).
add_library(c_roblib STATIC 
        library.c
        src/collections.c
        src/ref_objects.c
        src/memory/memory_pool.c
        src/string/string_builder.c
        src/string/string_utils.c
        src/map/hashmap.c
        src/list/array_list.c
)

# Allow targets to easily include headers from the src directory
target_include_directories(c_roblib PUBLIC src)

add_executable( json_parse
        src/json_parser/json_parser.c
        src/json_parser/arena.c
        src/json_parser/error_result.c
)
target_link_libraries(json_parse PRIVATE c_roblib)
target_compile_options(json_parse PRIVATE -Wno-unused-function)

# Dedicated executable for bitset to allow running it via CMake with C23 settings
add_executable(bitset_test_run src/set/bitset.c)
target_link_libraries(bitset_test_run PRIVATE c_roblib)