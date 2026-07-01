cmake_minimum_required(VERSION 4.2)
project(c_roblib C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Ensure the library is usable in various contexts
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Define semantic paths to avoid confusion with CMAKE_CURRENT_SOURCE_DIR
set(LIB_ROOT_DIR    ${CMAKE_CURRENT_SOURCE_DIR})
set(LIB_INCLUDE_DIR ${LIB_ROOT_DIR}/include)
set(LIB_SRC_DIR     ${LIB_ROOT_DIR}/src)

# Set a default build type if none is specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Helpful for debugging (CLion usually does this in Debug, but explicit is fine)
# -fsanitize=address -fsanitize=leak
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g -fno-omit-frame-pointer -fsanitize=address -fsanitize=leak \
-Wall -Wpedantic -Wextra -Werror \
-Wno-unknown-attributes -Wno-unused-const-variable -Wno-gnu-zero-variadic-macro-arguments -Wno-gnu-include-next \
-Wno-unused-parameter \
")

# Use add_compile_options with generator expressions to robustly add release flags
# This ensures they are applied even if the cache has old values.
add_compile_options($<$<CONFIG:Release>:-O3>)
add_compile_options($<$<CONFIG:Release>:-march=native>)
add_compile_options($<$<CONFIG:Release>:-DNDEBUG>)

# 1. Consolidate all library source files
set(LIB_SOURCES
        ${LIB_SRC_DIR}/collections.c
        ${LIB_SRC_DIR}/ref_objects.c
        ${LIB_SRC_DIR}/memory/memory_pool.c
        ${LIB_SRC_DIR}/string/string_builder.c
        ${LIB_SRC_DIR}/string/string_utils.c
        ${LIB_SRC_DIR}/map/hashmap.c
        ${LIB_SRC_DIR}/list/array_list.c
        ${LIB_SRC_DIR}/set/bitset.c
        ${LIB_SRC_DIR}/json_parser/json_parser.c
        ${LIB_SRC_DIR}/json_parser/arena.c
        ${LIB_SRC_DIR}/json_parser/error_result.c
        ${LIB_SRC_DIR}/json_parser/lookup_table.c
)

add_library(c_roblib STATIC ${LIB_SOURCES})

# 2. Organize Header Search Paths
# PUBLIC: headers in 'include' are needed by anyone linking to this library.
# PRIVATE: headers in 'src' (subfolders) are only used for building the library itself.
target_include_directories(c_roblib
    PUBLIC
        # This allows us to use #include "roblib_all.h" inside the library
        # but also supports the "roblib/roblib_all.h" style if we move them
        $<BUILD_INTERFACE:${LIB_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${LIB_SRC_DIR}
)

# JSON parser executable (now just links against the library)
add_executable(json_parse ${LIB_SRC_DIR}/json_parser/json_parser.c) # Assumes you move main logic here
target_link_libraries(json_parse PRIVATE c_roblib)
target_compile_options(json_parse PRIVATE -DJSON_PARSER_MAIN -Wno-unused-function)

# 3. Automated Installation (Solves the manual copy-paste problem)
# You can run: cmake --install . --prefix ../other_project/libs
# e.g.
# cmake --install cmake-build-debug --prefix "/Users/robross/Documents/Development/CLionProjects/text_adventures/external/roblib"
install(TARGETS c_roblib
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)

install(DIRECTORY "${LIB_INCLUDE_DIR}/"
        DESTINATION include
        FILES_MATCHING PATTERN "*.h")
