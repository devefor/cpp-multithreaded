#include "process_pool.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>

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

int main() {
    processpool::ProcessPool pool(4);

    auto f1 = pool.Submit(&Square, 12);
    auto f2 = pool.Submit(&SumRange, 1, 1'000'000);
    auto f3 = pool.Submit(&ValidatePositive, 42);
    auto f4 = pool.Submit(&ValidatePositive, -1);

    std::cout << "Square(12) = " << f1.Get() << '\n';
    std::cout << "SumRange(1, 1'000'000) = " << f2.Get() << '\n';
    f3.Get();
    std::cout << "ValidatePositive(42) completed successfully\n";

    try {
        f4.Get();
    } catch (const std::exception& e) {
        std::cout << "Task failed as expected: " << e.what() << '\n';
    }

    return 0;
}
