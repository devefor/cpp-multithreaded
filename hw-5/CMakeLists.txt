cmake_minimum_required(VERSION 3.25)

project(mpsc_queue LANGUAGES CXX)

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

add_library(mpsc_queue
    mpsc_queue.cpp
)
target_include_directories(mpsc_queue
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(mpsc_consumer
    consumer.cpp
)
target_link_libraries(mpsc_consumer
    PRIVATE
    mpsc_queue
)

add_executable(mpsc_producer_first
    producer_first.cpp
)
target_link_libraries(mpsc_producer_first
    PRIVATE
    mpsc_queue
)
add_executable(mpsc_producer_second
    producer_second.cpp
)
target_link_libraries(mpsc_producer_second
    PRIVATE
    mpsc_queue
)
add_executable(mpsc_producer_stop
    producer_stop.cpp
)
target_link_libraries(mpsc_producer_stop
    PRIVATE
    mpsc_queue
)
# add_executable(mpsc_cleanup
#     cleanup.cpp
# )
# target_link_libraries(mpsc_cleanup
#     PRIVATE
#     mpsc_queue
# )

add_executable(mpsc_queue_test
    test.cpp
)
target_link_libraries(mpsc_queue_test
    PRIVATE
    mpsc_queue
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
)

add_executable(mpsc_queue_benchmark
    benchmark.cpp
)
target_link_libraries(mpsc_queue_benchmark
    PRIVATE
    mpsc_queue
    Threads::Threads
    benchmark::benchmark
)