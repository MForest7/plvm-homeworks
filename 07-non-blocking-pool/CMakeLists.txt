cmake_minimum_required(VERSION 3.25)
project(07-non-blocking-pool)

add_compile_options(-O2 -std=c++20)

add_executable(non-blocking-pool src/main.cpp)
