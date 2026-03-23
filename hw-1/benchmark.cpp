#include "dfs.h"

#include <benchmark/benchmark.h>

#include <vector>

namespace {

Graph MakeLineGraph(int n) {
    Graph graph;
    graph.adj.resize(n);
    for (int i = 0; i + 1 < n; ++i) {
        graph.adj[i].push_back(i + 1);
        graph.adj[i + 1].push_back(i);
    }
    return graph;
}

Graph MakeBinaryTreeGraph(int levels) {
    const int n = (1 << levels) - 1;
    Graph graph;
    graph.adj.resize(n);

    for (int parent = 0; parent < n; ++parent) {
        const int left = parent * 2 + 1;
        const int right = parent * 2 + 2;
        if (left < n) {
            graph.adj[parent].push_back(left);
            graph.adj[left].push_back(parent);
        }
        if (right < n) {
            graph.adj[parent].push_back(right);
            graph.adj[right].push_back(parent);
        }
    }
    return graph;
}

Graph MakeDenseGraph(int n) {
    Graph graph;
    graph.adj.resize(n);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i != j) {
                graph.adj[i].push_back(j);
            }
        }
    }
    return graph;
}

}  // namespace

static void BM_CooperativeDFS_LineGraph(benchmark::State& state) {
    const Graph graph = MakeLineGraph(static_cast<int>(state.range(0)));

    for (auto _ : state) {
        const auto order = CooperativeDFS(graph);
        benchmark::DoNotOptimize(order.data());
        benchmark::DoNotOptimize(order.size());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_CooperativeDFS_LineGraph)->Arg(128)->Arg(1024)->Arg(4096);

static void BM_CooperativeDFS_BinaryTree(benchmark::State& state) {
    const Graph graph = MakeBinaryTreeGraph(static_cast<int>(state.range(0)));

    for (auto _ : state) {
        const auto order = CooperativeDFS(graph);
        benchmark::DoNotOptimize(order.data());
        benchmark::DoNotOptimize(order.size());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(graph.adj.size()));
}
BENCHMARK(BM_CooperativeDFS_BinaryTree)->Arg(8)->Arg(10)->Arg(12);

static void BM_CooperativeDFS_DenseGraph(benchmark::State& state) {
    const Graph graph = MakeDenseGraph(static_cast<int>(state.range(0)));

    for (auto _ : state) {
        const auto order = CooperativeDFS(graph);
        benchmark::DoNotOptimize(order.data());
        benchmark::DoNotOptimize(order.size());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_CooperativeDFS_DenseGraph)->Arg(32)->Arg(64)->Arg(96);

BENCHMARK_MAIN();
