cmake_minimum_required(VERSION 3.25)

project(mutex LANGUAGES CXX)

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
FetchContent_MakeAvailable(googletest)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.5
)
FetchContent_MakeAvailable(googlebenchmark)

add_library(mutex_lib
    mutex.cpp
)
target_include_directories(mutex_lib
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(mutex_test
    test.cpp
)
target_link_libraries(mutex_test 
    PRIVATE
    mutex_lib
    Threads::Threads 
    GTest::gtest 
    GTest::gtest_main
)

add_executable(mutex_benchmark
    benchmark.cpp
)
target_link_libraries(mutex_benchmark
    PRIVATE
    mutex_lib
    Threads::Threads
    benchmark::benchmark
)
