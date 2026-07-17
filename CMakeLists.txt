cmake_minimum_required(VERSION 4.2)
project(c_roblib C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Ensure the library is usable in various contexts
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Define semantic paths to avoid confusion with CMAKE_CURRENT_SOURCE_DIR
set(LIB_ROOT_DIR    ${CMAKE_CURRENT_SOURCE_DIR})
set(LIB_INCLUDE_DIR ${LIB_ROOT_DIR}/include)
set(LIB_SRC_DIR     ${LIB_ROOT_DIR}/src)
set(LIB_TEST_DIR    ${LIB_ROOT_DIR}/test)

# Set a default build type if none is specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Helpful for debugging (CLion usually does this in Debug, but explicit is fine)
# -fsanitize=address -fsanitize=leak
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g -fno-omit-frame-pointer \
-fsanitize=address -fsanitize=leak \
-Wall -Wpedantic -Wextra -Werror \
-Wno-unknown-attributes \
-Wno-unused-const-variable \
-Wno-gnu-zero-variadic-macro-arguments \
-Wno-gnu-include-next \
-Wno-unused-parameter \
-Wno-unused-function \
-Wno-unused-variable \
-Wno-gnu-anonymous-struct \
-Wno-nested-anon-types \
")

# Synchronize C++ flags for the test runner and set standard
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use add_compile_options with generator expressions to robustly add release flags
# This ensures they are applied even if the cache has old values.
add_compile_options($<$<CONFIG:Release>:-O3>)
add_compile_options($<$<CONFIG:Release>:-march=native>)
add_compile_options($<$<CONFIG:Release>:-DNDEBUG>)

if (MSVC)
    add_compile_options(/utf-8)
else()
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

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
        ${LIB_SRC_DIR}/unicode/unicode_tools.c
        ${LIB_SRC_DIR}/file_io/file_utils.c
        ${LIB_SRC_DIR}/set/bitset.c
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

add_executable(json_parse2
        ${LIB_SRC_DIR}/json_parser/json_parser.c
        ${LIB_SRC_DIR}/json_parser/arena.c
        ${LIB_SRC_DIR}/string/string_builder.c
        ${LIB_SRC_DIR}/unicode/unicode_tools.c
        ${LIB_SRC_DIR}/json_parser/error_result.c
)
#target_link_libraries(json_parse2 PRIVATE c_roblib)
target_include_directories(json_parse2
        PUBLIC
        PRIVATE
        ${LIB_INCLUDE_DIR}
        ${LIB_SRC_DIR}
)
target_compile_options(json_parse2 PRIVATE -DJSON_PARSER_2_MAIN -Wno-unused-function -Wno-unused-variable )

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


# Gtest
#-------
#
# to run all tests:
# ./cmake-build-debug/run_tests


# 1. Automatically download and "install" GoogleTest into the project
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/heads/main.zip
        # This is the modern (CMake 3.28+) way to keep it out of your 'install' command
        EXCLUDE_FROM_ALL
)

# Prevent GoogleTest from installing its own binaries even if EXCLUDE_FROM_ALL is missed
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
# Since you only link 'gtest', we can disable 'gmock' to speed up the build and avoid its install rules
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# 2. Find all test source files recursively in the test directory
file(GLOB_RECURSE TEST_SOURCES "${LIB_TEST_DIR}/*.cpp")

# Define your test executable with all discovered source files
add_executable(run_tests ${TEST_SOURCES})

# Define a macro so the code knows where the test data is located.
# CMAKE_CURRENT_SOURCE_DIR points to the directory containing this CMakeLists.txt.
target_compile_definitions(run_tests PRIVATE
        JSONTestSuite_DATA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/test/json_parser/JSONTestSuite"
        ERROR_REPORTING_DATA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/test/json_parser/error_reporting")

# 3. Link your static C library AND GoogleTest to the test executable
target_link_libraries(run_tests
        PRIVATE
        gtest
        c_roblib
)

# 4. Enable discovery so CMake knows this is a test suite
include(GoogleTest)
gtest_discover_tests(run_tests)
