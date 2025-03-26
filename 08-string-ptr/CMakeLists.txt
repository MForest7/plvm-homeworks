cmake_minimum_required(VERSION 3.14)
project(string_ptr)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

FetchContent_MakeAvailable(googletest)

set(SOURCES
    src/cstring.cpp
    src/string_ptr.cpp
    )

add_library(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/src)

enable_testing()

add_executable(
    test_string_ptr
    tests/test_string_ptr.cpp
)

target_link_libraries(
    test_string_ptr
    string_ptr
    GTest::gtest_main
)

target_include_directories(test_string_ptr PUBLIC ${PROJECT_SOURCE_DIR}/src)

include(GoogleTest)
gtest_discover_tests(test_string_ptr)
