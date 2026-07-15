# roblib

My slowly-evolving library of useful (to me) C functions. See project_info.txt for environment details.
This has only been tested on my Mac. It's mostly C23 but I do use a few POSIX calls, especially mmap for
the Arena, file IO, etc. So this currently won't build on Windows as-is.

# Build

> cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
> 
> cmake --build cmake-build-debug


# Test

From the project root : 
> ./cmake-build-debug/run_tests


# Install

To add the library and headers to your project, from the project root (of this project):
> cmake --install cmake-build-debug --prefix .../your_project/your_libdirectory
