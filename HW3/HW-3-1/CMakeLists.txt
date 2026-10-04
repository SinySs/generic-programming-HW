cmake_minimum_required(VERSION 3.16)
project(nth_power LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(nth_power INTERFACE)
target_include_directories(nth_power INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(GTest REQUIRED)

add_executable(test_nth_power test/test_nth_power.cpp)
target_link_libraries(test_nth_power PRIVATE nth_power GTest::gtest_main)

if(MSVC)
    target_compile_options(test_nth_power PRIVATE /W4 /permissive-)
else()
    target_compile_options(test_nth_power PRIVATE -Wall -Wextra -Wpedantic)
endif()
