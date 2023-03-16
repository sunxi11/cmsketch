cmake_minimum_required(VERSION 3.24)
project(c_sketch C)

set(CMAKE_C_STANDARD 11)
include_directories(cmsketch)
add_executable(c_sketch main.c cmsketch/cmsketch.c)
