cmake_minimum_required(VERSION 4.2)
project(c_roblib C)

set(CMAKE_C_STANDARD 23)

add_library(c_roblib STATIC library.c)
