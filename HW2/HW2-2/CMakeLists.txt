cmake_minimum_required(VERSION 3.16)
project(hw2_2_string_twine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(string_twine INTERFACE)
target_include_directories(string_twine INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(GTest REQUIRED)

add_executable(twine_tests src/tests.cpp)
target_link_libraries(twine_tests PRIVATE string_twine GTest::gtest_main)
target_compile_options(twine_tests PRIVATE -Wall -Wextra -Wpedantic)

include(GoogleTest)
gtest_discover_tests(twine_tests)

option(TWINE_SANITIZE "Build tests with AddressSanitizer and UBSan" OFF)
if(TWINE_SANITIZE)
  target_compile_options(twine_tests PRIVATE -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(twine_tests PRIVATE -fsanitize=address,undefined)
endif()
