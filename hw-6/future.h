#pragma once

#include "detail.h"

#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace processpool {

// Future<T> - simplified analogue of std::future<T>
template <typename T>
class Future {
private:
    // Shared state that also used by ProcessPool internals
    std::shared_ptr<detail::SharedState<T>> state_;
public:
    Future() = default;
    // Future is constructed from shared state created ProcessPool
    explicit Future(std::shared_ptr<detail::SharedState<T>> state)
        : state_(std::move(state)) {}

    // Copy is forbidden, moving is allowed
    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;
    
    Future(Future&&) noexcept = default;
    Future& operator=(Future&&) noexcept = default;

    // Checks this Future is connected to some shared stat
    // (default-constructed Future is invalid until a real state is assigned)
    bool Valid() const noexcept {
        return static_cast<bool>(state_);
    }

    // Blocks the caller until the result becomes ready
    void Wait() const {
        EnsureValid();
        std::unique_lock lock(state_->mutex);
        state_->cv.wait(lock, [this] { return state_->ready; });
    }

    // Non-blocking readiness check
    bool IsReady() const {
        EnsureValid();
        std::lock_guard lock(state_->mutex);
        return state_->ready;
    }

    // Waits for completion and then returns the result (analogue of std::future<T>::get())
    T Get() {
        Wait();
        std::lock_guard lock(state_->mutex);
        if (state_->error) {
            std::rethrow_exception(state_->error);
        }
        return std::move(*state_->value);
    }
private:
    // Protect against using an empty/default Future
    void EnsureValid() const {
        if (!state_) {
            throw std::logic_error("future has no shared state");
        }
    }
};

// Specialization for tasks that return void
template <>
class Future<void> {
private:
    std::shared_ptr<detail::SharedState<void>> state_;
public:
    Future() = default;
    explicit Future(std::shared_ptr<detail::SharedState<void>> state)
        : state_(std::move(state)) {}

    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;

    Future(Future&&) noexcept = default;
    Future& operator=(Future&&) noexcept = default;

    bool Valid() const noexcept {
        return static_cast<bool>(state_);
    }

    void Wait() const {
        EnsureValid();
        std::unique_lock lock(state_->mutex);
        state_->cv.wait(lock, [this] { return state_->ready; });
    }

    bool IsReady() const {
        EnsureValid();
        std::lock_guard lock(state_->mutex);
        return state_->ready;
    }

    void Get() {
        Wait();
        std::lock_guard lock(state_->mutex);
        if (state_->error) {
            std::rethrow_exception(state_->error);
        }
    }
private:
    void EnsureValid() const {
        if (!state_) {
            throw std::logic_error("future has no shared state");
        }
    }
};

}  // namespace processpool