#include "apply_function.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

TEST(ApplyFunctionTest, EmptyVectorDoesNotFail) {
    std::vector<int> data;
    ApplyFunction<int>(data, [](int& x) { x += 1; }, 4);
    EXPECT_TRUE(data.empty());
}

TEST(ApplyFunctionTest, SingleThreadTransformsAllElements) {
    std::vector<int> data{1, 2, 3, 4, 5};

    ApplyFunction<int>(data, [](int& x) { x *= 2; }, 1);

    EXPECT_EQ(data, (std::vector<int>{2, 4, 6, 8, 10}));
}

TEST(ApplyFunctionTest, MultiThreadTransformsAllElements) {
    std::vector<int> data{1, 2, 3, 4, 5, 6, 7, 8};

    ApplyFunction<int>(data, [](int& x) { x += 10; }, 4);

    EXPECT_EQ(data, (std::vector<int>{11, 12, 13, 14, 15, 16, 17, 18}));
}

TEST(ApplyFunctionTest, ThreadCountGreaterThanDataSize) {
    std::vector<int> data{1, 2, 3};

    ApplyFunction<int>(data, [](int& x) { x = x * x; }, 100);

    EXPECT_EQ(data, (std::vector<int>{1, 4, 9}));
}

TEST(ApplyFunctionTest, UnevenSplitWorksCorrectly) {
    std::vector<int> data{0, 1, 2, 3, 4, 5, 6};

    ApplyFunction<int>(data, [](int& x) { x += 1; }, 3);

    EXPECT_EQ(data, (std::vector<int>{1, 2, 3, 4, 5, 6, 7}));
}

TEST(ApplyFunctionTest, WorksWithStrings) {
    std::vector<std::string> data{"a", "bb", "ccc"};

    ApplyFunction<std::string>(data, [](std::string& s) { s += "!"; }, 2);

    EXPECT_EQ(data, (std::vector<std::string>{"a!", "bb!", "ccc!"}));
}

TEST(ApplyFunctionTest, NonPositiveThreadCountFallsBackToSingleThread) {
    std::vector<int> data{1, 2, 3};

    ApplyFunction<int>(data, [](int& x) { x += 5; }, 0);

    EXPECT_EQ(data, (std::vector<int>{6, 7, 8}));
}