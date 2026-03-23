#include "process_pool.h"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <vector>

int Square(int value) {
    return value * value;
}

int Add(int a, int b) {
    return a + b;
}

std::int64_t SumRange(int from, int to) {
    std::int64_t sum = 0;
    for (int i = from; i <= to; ++i) {
        sum += i;
    }
    return sum;
}

// Measure one simple round-trip: Submit + Get.
static void BM_SubmitGet_Square(benchmark::State& state) {
    const int process_count = static_cast<int>(state.range(0));
    processpool::ProcessPool pool(process_count);

    for (auto _ : state) {
        auto future = pool.Submit(&Square, 123);
        benchmark::DoNotOptimize(future.Get());
    }
}

BENCHMARK(BM_SubmitGet_Square)->Arg(1)->Arg(2)->Arg(4)->Arg(8);

// Measure batch execution of many small tasks.
static void BM_BatchSubmitGet_Add(benchmark::State& state) {
    const int process_count = static_cast<int>(state.range(0));
    const int task_count = static_cast<int>(state.range(1));
    processpool::ProcessPool pool(process_count);

    for (auto _ : state) {
        std::vector<processpool::Future<int>> futures;
        futures.reserve(task_count);

        for (int i = 0; i < task_count; ++i) {
            futures.push_back(pool.Submit(&Add, i, i + 1));
        }

        for (int i = 0; i < task_count; ++i) {
            benchmark::DoNotOptimize(futures[i].Get());
        }
    }

    state.SetItemsProcessed(state.iterations() * task_count);
}

BENCHMARK(BM_BatchSubmitGet_Add)
    ->Args({1, 10})
    ->Args({2, 10})
    ->Args({4, 10})
    ->Args({4, 100})
    ->Args({8, 100});

// Measure task with more CPU work inside worker.
static void BM_SubmitGet_SumRange(benchmark::State& state) {
    const int process_count = static_cast<int>(state.range(0));
    const int upper = static_cast<int>(state.range(1));
    processpool::ProcessPool pool(process_count);

    for (auto _ : state) {
        auto future = pool.Submit(&SumRange, 1, upper);
        benchmark::DoNotOptimize(future.Get());
    }
}

BENCHMARK(BM_SubmitGet_SumRange)
    ->Args({1, 1000})
    ->Args({2, 1000})
    ->Args({4, 1000})
    ->Args({4, 100000});

BENCHMARK_MAIN();