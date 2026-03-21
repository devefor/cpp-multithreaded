cmake_minimum_required(VERSION 3.25)

project(hw-2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)

FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.5
)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest googlebenchmark)

add_library(apply_function INTERFACE)
target_include_directories(apply_function INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(apply_function_tests
    apply_function_test.cpp
)
target_link_libraries(apply_function_tests
    PRIVATE
    apply_function
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
)

add_executable(apply_function_benchmark
    apply_function_benchmark.cpp
)
target_link_libraries(apply_function_benchmark
    PRIVATE
    apply_function
    Threads::Threads
    benchmark::benchmark
)