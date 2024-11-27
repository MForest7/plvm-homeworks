cmake_minimum_required(VERSION 3.25)
project(plvm3-lama-bcstats)

set(CMAKE_CXX_FLAGS -m32)
add_compile_options(-m32 -O2 -g)

add_executable(bcstats src/bcstats.cpp src/bytefile.cpp)
