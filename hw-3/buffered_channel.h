#pragma once

#include <cstddef>
#include <vector>
#include <mutex>
#include <optional>
#include <condition_variable>
#include <stdexcept>

template <class T>
class BufferedChannel {
private:
    std::size_t capacity;
    std::mutex mtx;
    bool closed = false;

    std::vector<T> buffer; // FIFO
    std::size_t head = 0;
    std::size_t tail = 0;
    std::size_t size = 0;

    std::condition_variable notEmpty;
    std::condition_variable notFull;
public:
    explicit BufferedChannel(int size)
        : capacity(size > 0 ? static_cast<std::size_t>(size) : 1),
        buffer(capacity) {}

    void Send(const T& value) {
        std::unique_lock<std::mutex> lock(mtx);
        // Check status of channel
        notFull.wait(lock, [this]() {
            return closed || size < capacity;
        });

        if (closed) {
            throw std::runtime_error("Send to close channel");
        }

        // Add item and wake up 1
        buffer[tail] = value;
        tail = (tail + 1) % capacity;
        ++size;
        lock.unlock();
        notEmpty.notify_one();
    }

    std::optional<T> Recv() {
        std::unique_lock<std::mutex> lock(mtx);
        // Check status of channel
        notEmpty.wait(lock, [this]() {
            return closed ||  size > 0;
        });

        // No wait -> queue is empty -> channel close 
        if (size == 0) {
            return std::nullopt;
        }

        // Return value and wake up 1
        T value = std::move(buffer[head]);
        head = (head + 1) % capacity;
        --size;

        lock.unlock();
        notFull.notify_one();
        return value;
    }

    void Close() {
        std::unique_lock<std::mutex> lock(mtx);
        if (closed) return;
        closed = true;

        lock.unlock();
        notEmpty.notify_all(); // receivers have nullopt
        notFull.notify_all(); // senders have runtime error
    }
};
