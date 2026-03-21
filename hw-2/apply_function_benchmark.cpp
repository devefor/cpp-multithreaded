#include "apply_function.hpp"

#include <benchmark/benchmark.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace {

    void CheapTransform(std::uint64_t& x) {
        x += x;
        benchmark::DoNotOptimize(x);
    }

    void HeavyTransform(double& x) {
        double value = x;
        for (int i = 0; i < 200; ++i) {
            value = std::sin(value) + std::cos(value) + std::sqrt(value + 2.0);
        }
        x = value;
        benchmark::DoNotOptimize(x);
    }

}

static void BM_SingleThreadForCheapWorkSmallVector(benchmark::State& state) {
    const std::size_t size = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        std::vector<std::uint64_t> data(size, 1);

        ApplyFunction<std::uint64_t>(data, CheapTransform, 1);

        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_SingleThreadForCheapWorkSmallVector)->Arg(16)->Arg(32)->Arg(64);

static void BM_MultiThreadForCheapWorkSmallVector(benchmark::State& state) {
    const std::size_t size = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        std::vector<std::uint64_t> data(size, 1);

        ApplyFunction<std::uint64_t>(data, CheapTransform, 4);

        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_MultiThreadForCheapWorkSmallVector)->Arg(16)->Arg(32)->Arg(64);

static void BM_SingleThreadForCheapWorkLargeVector(benchmark::State& state) {
    const std::size_t size = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        std::vector<std::uint64_t> data(size, 1);
        ApplyFunction<std::uint64_t>(data, CheapTransform, 1);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_SingleThreadForCheapWorkLargeVector)->Arg(100000)->Arg(1000000);

static void BM_MultiThreadForCheapWorkLargeVector(benchmark::State& state) {
    const std::size_t size = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        std::vector<std::uint64_t> data(size, 1);
        ApplyFunction<std::uint64_t>(data, CheapTransform, 4);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_MultiThreadForCheapWorkLargeVector)->Arg(100000)->Arg(1000000);

static void BM_SingleThreadForHeavyWorkSmallVector(benchmark::State& state) {
    const std::size_t size = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        std::vector<double> data(size, 1.2345);

        ApplyFunction<double>(data, HeavyTransform, 1);

        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_SingleThreadForHeavyWorkSmallVector)->Arg(8)->Arg(16);

static void BM_MultiThreadForHeavyWorkSmallVector(benchmark::State& state) {
    const std::size_t size = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        std::vector<double> data(size, 1.2345);

        ApplyFunction<double>(data, HeavyTransform, 4);

        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_MultiThreadForHeavyWorkSmallVector)->Arg(8)->Arg(16);

static void BM_SingleThreadForHeavyWorkLargeVector(benchmark::State& state) {
    const std::size_t size = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        std::vector<double> data(size, 1.2345);

        ApplyFunction<double>(data, HeavyTransform, 1);

        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_SingleThreadForHeavyWorkLargeVector)->Arg(10000)->Arg(100000);

static void BM_MultiThreadForHeavyWorkLargeVector(benchmark::State& state) {
    const std::size_t size = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        std::vector<double> data(size, 1.2345);

        ApplyFunction<double>(data, HeavyTransform, 4);

        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_MultiThreadForHeavyWorkLargeVector)->Arg(10000)->Arg(100000);

BENCHMARK_MAIN();