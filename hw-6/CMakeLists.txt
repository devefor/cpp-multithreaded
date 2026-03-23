cmake_minimum_required(VERSION 3.25)

project(process_pool LANGUAGES CXX)

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

add_executable(process_pool
    main.cpp
    process_pool.cpp
)
target_include_directories(process_pool
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(process_pool_lib
    process_pool.cpp
)

target_include_directories(process_pool_lib
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(process_pool_test
    test.cpp
)
target_link_libraries(process_pool_test
    PRIVATE
    process_pool_lib
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
)

add_executable(process_pool_benchmark
    benchmark.cpp
)
target_link_libraries(process_pool_benchmark
    PRIVATE
    process_pool_lib
    Threads::Threads
    benchmark::benchmark
)