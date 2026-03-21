#pragma once

#include <functional>
#include <thread>
#include <vector>

template <typename T>
void ApplyFunction(std::vector<T>& data, const std::function<void(T&)>& transform, const int threadCount = 1) {
    if (data.empty()) {
        return;
    }

    // Check correct threadCount
    const std::size_t checkThreadCount = threadCount <= 0 ? 1 : static_cast<std::size_t>(threadCount);
    const std::size_t actualThreadCount = checkThreadCount > data.size() ? data.size() : checkThreadCount;

    // Single thread
    if (actualThreadCount == 1) {
        for (auto& item : data) {
            transform(item);
        }
        return;
    }

    std::vector<std::thread> threads;
    threads.reserve(actualThreadCount);
    const std::size_t initialChunkSize = data.size() / actualThreadCount;
    const std::size_t remainItem = data.size() % actualThreadCount;

    // Make threads
    std::size_t startIndexItemThread = 0;
    for (std::size_t i = 0; i < actualThreadCount; ++i) {
        const std::size_t chunkSize = initialChunkSize + (i < remainItem ? 1 : 0);
        const std::size_t endIndexItemThread = startIndexItemThread + chunkSize;

        // Start thread and add sequence lambda functions 
        threads.emplace_back([startIndexItemThread, endIndexItemThread, &data, &transform]() {
            for (std::size_t j = startIndexItemThread; j < endIndexItemThread; ++j) {
                transform(data[j]);
            }
        });

        startIndexItemThread = endIndexItemThread;
    }

    // Wait multithreads
    for (auto& thread : threads) {
        thread.join();
    }
}
