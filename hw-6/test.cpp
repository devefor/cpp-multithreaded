#include "process_pool.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

int Square(int value) {
    return value * value;
}

std::int64_t SumRange(int from, int to) {
    std::int64_t sum = 0;
    for (int i = from; i <= to; ++i) {
        sum += i;
    }
    return sum;
}

void ValidatePositive(int value) {
    if (value < 0) {
        throw std::runtime_error("value must be non-negative");
    }
}

int SleepAndReturn(int usec, int value) {
    ::usleep(static_cast<useconds_t>(usec));
    return value;
}

int Add(int a, int b) {
    return a + b;
}

TEST(ProcessPoolTest, ConstructorThrowsOnZeroProcesses) {
    EXPECT_THROW(processpool::ProcessPool pool(0), std::invalid_argument);
}

TEST(ProcessPoolTest, SubmitReturnsCorrectValue) {
    processpool::ProcessPool pool(2);

    auto future = pool.Submit(&Square, 12);

    EXPECT_TRUE(future.Valid());
    EXPECT_EQ(future.Get(), 144);
}

TEST(ProcessPoolTest, SubmitVoidTaskWorks) {
    processpool::ProcessPool pool(2);

    auto future = pool.Submit(&ValidatePositive, 42);

    EXPECT_TRUE(future.Valid());
    EXPECT_NO_THROW(future.Get());
}

TEST(ProcessPoolTest, ExceptionIsPropagatedThroughFuture) {
    processpool::ProcessPool pool(2);

    auto future = pool.Submit(&ValidatePositive, -1);

    EXPECT_THROW(
        {
            try {
                future.Get();
            } catch (const std::runtime_error& e) {
                EXPECT_STREQ(e.what(), "value must be non-negative");
                throw;
            }
        },
        std::runtime_error
    );
}

TEST(ProcessPoolTest, ManyTasksReturnCorrectResults) {
    processpool::ProcessPool pool(4);

    std::vector<processpool::Future<int>> futures;
    futures.reserve(100);

    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool.Submit(&Square, i));
    }

    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(futures[i].Get(), i * i);
    }
}

TEST(ProcessPoolTest, MixedTasksReturnCorrectResults) {
    processpool::ProcessPool pool(4);

    auto f1 = pool.Submit(&Square, 9);
    auto f2 = pool.Submit(&SumRange, 1, 1000);
    auto f3 = pool.Submit(&Add, 123, 456);

    EXPECT_EQ(f1.Get(), 81);
    EXPECT_EQ(f2.Get(), 500500);
    EXPECT_EQ(f3.Get(), 579);
}

TEST(ProcessPoolTest, FutureIsReadyChangesAfterCompletion) {
    processpool::ProcessPool pool(2);

    auto future = pool.Submit(&SleepAndReturn, 100000, 7); // 100 ms

    EXPECT_FALSE(future.IsReady());

    future.Wait();

    EXPECT_TRUE(future.IsReady());
    EXPECT_EQ(future.Get(), 7);
}

TEST(ProcessPoolTest, ShutdownCanBeCalledTwice) {
    processpool::ProcessPool pool(2);

    auto future = pool.Submit(&Square, 5);
    EXPECT_EQ(future.Get(), 25);

    EXPECT_NO_THROW(pool.Shutdown());
    EXPECT_NO_THROW(pool.Shutdown());
}

TEST(ProcessPoolTest, SumRangeLargeInputWorks) {
    processpool::ProcessPool pool(4);

    auto future = pool.Submit(&SumRange, 1, 1000000);

    EXPECT_EQ(future.Get(), 500000500000LL);
}