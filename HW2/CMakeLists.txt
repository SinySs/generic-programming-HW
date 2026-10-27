cmake_minimum_required(VERSION 3.16)
project(hw2_1_cow_string LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(cow_string INTERFACE)
target_include_directories(cow_string INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(GTest REQUIRED)

add_executable(cow_tests src/tests.cpp)
target_link_libraries(cow_tests PRIVATE cow_string GTest::gtest_main)
target_compile_options(cow_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(cow_bench src/benchmark.cpp)
target_link_libraries(cow_bench PRIVATE cow_string)
target_compile_options(cow_bench PRIVATE -Wall -Wextra -Wpedantic -O3 -DNDEBUG)

include(GoogleTest)
gtest_discover_tests(cow_tests)

option(COW_SANITIZE "Build tests with AddressSanitizer and UBSan" OFF)
if(COW_SANITIZE)
  target_compile_options(cow_tests PRIVATE -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(cow_tests PRIVATE -fsanitize=address,undefined)
endif()
