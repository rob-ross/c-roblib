# roblib

My slowly-evolving library of useful (to me) C functions. See project_info.txt for environment details.

# Build

> cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
> 
> cmake --build cmake-build-debug


# Test

From the project root : 
> ./cmake-build-debug/run_tests


# Install

To add the library and headers to your project, from the project root:
> cmake --install cmake-build-debug --prefix .../your_project/your_libdirectory
