[![CI](https://github.com/rob-ross/c-roblib/actions/workflows/ci.yml/badge.svg)](https://github.com/rob-ross/c-roblib/actions/workflows/ci.yml)
# roblib

My slowly-evolving library of useful (to me) C functions. See project_info.txt for environment details.
This has only been tested on my Mac. It's mostly C23 but I do use a few POSIX calls, especially mmap for
the Arena, file IO, etc. So this currently won't build on Windows as-is.

## Requirements
- CMake 3.28 or newer
- A C23 compiler for the C library code
- C++23 compiler for the gtest test cases
- macOS or Linux
- not Windows

## Build
    Has built successfully on:
    - Mac Intel/ARM with Clang 17+
    - Mac ARM with gcc 15

     > cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
     > cmake --build cmake-build-debug --target c_roblib


## Test
From the project root:

    > cmake --build cmake-build-debug --target run_tests
    > ./cmake-build-debug/run_tests



## Install

To add the library and headers to your project, from the project root (of this project):

    > cmake --install cmake-build-debug --prefix .../your_project/your_libdirectory
