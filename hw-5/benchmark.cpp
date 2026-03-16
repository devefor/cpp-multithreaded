#include "mpsc_queue.h"

#include <benchmark/benchmark.h>
#include <cerrno>
#include <sys/mman.h>

using namespace mpsc_queue;

namespace {
    constexpr const char* kBenchShmPath = "/mpsc_queue_benchmark_shm";
    constexpr uint64_t kBenchShmSize = 1 << 20;

    void RemoveSharedMemoryIfExists(const char* path) {
        if (shm_unlink(path) == -1 && errno != ENOENT) {
            std::abort();
        }
    }
}

static void BM_ProducerSendText(benchmark::State& state) {
    RemoveSharedMemoryIfExists(kBenchShmPath);

    ProducerNode producer(kBenchShmPath, kBenchShmSize);
    ConsumerNode consumer(kBenchShmPath, kBenchShmSize, MessageType::Text);

    const std::string payload(static_cast<size_t>(state.range(0)), 'a');
    const auto* ptr = reinterpret_cast<const std::byte*>(payload.data());
    std::span<const std::byte> bytes{ptr, payload.size()};

    for (auto _ : state) {
        producer.Send(MessageType::Text, bytes);
        auto msg = consumer.Receive();
        benchmark::DoNotOptimize(msg);
    }

    state.SetBytesProcessed(
        static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(payload.size()));

    RemoveSharedMemoryIfExists(kBenchShmPath);
}

BENCHMARK(BM_ProducerSendText)->Arg(16)->Arg(64)->Arg(256)->Arg(1024);

BENCHMARK_MAIN();