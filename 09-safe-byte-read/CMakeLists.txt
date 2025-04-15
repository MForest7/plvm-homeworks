cmake_minimum_required(VERSION 3.14)
project(safe_read)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

FetchContent_MakeAvailable(googletest)

set(SOURCES
    src/safe_read.cpp
    )

add_library(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/src)

enable_testing()

add_executable(
    test_safe_read
    tests/test_safe_read.cpp
)

target_link_libraries(
    test_safe_read
    safe_read
    GTest::gtest_main
)

target_include_directories(test_safe_read PUBLIC ${PROJECT_SOURCE_DIR}/src)

include(GoogleTest)
gtest_discover_tests(test_safe_read)
