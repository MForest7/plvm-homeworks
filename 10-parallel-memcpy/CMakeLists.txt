cmake_minimum_required(VERSION 3.25)
project(10-parallel-memcpy)

add_compile_options(-O2 -std=c++20)

add_executable(parallel-memcpy src/main.cpp)
