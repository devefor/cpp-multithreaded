#pragma once

#include "detail.h"
#include "future.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sys/socket.h>
#include <sys/types.h>

namespace processpool {

// ProcessPool - pool of worker processes)))
// Tasks send from parent process to child processes through sockets
// The result - delivere back to parent process and store into Future shared state
class ProcessPool {
private:
    // Command sent from parent to worker
    enum class Command : std::uint32_t { Run = 1, Stop = 2 };
    // Status of result sent from worker back to parent
    enum class ResultStatus : std::uint32_t { Value = 1, Void = 2, Exception = 3 };

    // parent -> worker
    struct TaskHeader {
        Command command{};
        std::uint64_t task_id{};
        std::uintptr_t executor{};
        std::uint64_t payload_size{};
    };

    // worker -> parent
    struct ResultHeader {
        std::uint64_t task_id{};
        ResultStatus status{};
        std::uint64_t payload_size{};
    };

    // Base class for pending result handler
    struct PendingBase {
        virtual ~PendingBase() = default;
        virtual void SetValue(const std::vector<std::byte>& payload) = 0;
        virtual void SetException(const std::string& text) = 0;
    };

    // Pending handler for non-void result type
    template <typename T>
    struct PendingValue final : PendingBase {
        explicit PendingValue(std::shared_ptr<detail::SharedState<T>> s)
            : state(std::move(s)) {}

        void SetValue(const std::vector<std::byte>& payload) override {
            // Returned payload must contain exactly one object of type T
            if (payload.size() != sizeof(T)) {
                SetException("invalid payload size for result");
                return;
            }

            T value{};
            std::memcpy(&value, payload.data(), sizeof(T));
            {
                std::lock_guard lock(state->mutex);
                state->value = std::move(value);
                state->ready = true;
            }

            state->cv.notify_all();
        }

        void SetException(const std::string& text) override {
            {
                std::lock_guard lock(state->mutex);
                state->error = std::make_exception_ptr(std::runtime_error(text));
                state->ready = true;
            }
            state->cv.notify_all();
        }

        std::shared_ptr<detail::SharedState<T>> state;
    };

    // Pending handler for void result
    struct PendingVoid final : PendingBase {
        explicit PendingVoid(std::shared_ptr<detail::SharedState<void>> s)
            : state(std::move(s)) {}

        void SetValue(const std::vector<std::byte>&) override {
            {
                std::lock_guard lock(state->mutex);
                state->ready = true;
            }
            state->cv.notify_all();
        }

        void SetException(const std::string& text) override {
            {
                std::lock_guard lock(state->mutex);
                state->error = std::make_exception_ptr(std::runtime_error(text));
                state->ready = true;
            }
            state->cv.notify_all();
        }

        std::shared_ptr<detail::SharedState<void>> state;
    };

    // Executed inside worker process
    // Params:
    // - input bytes + size
    // - output buffer for serialized result
    // - string for exception text
    using ExecutorFn = void (*)(const std::byte*, std::size_t, std::vector<std::byte>&, std::string&);

    // Description of one worker process from parent-process point of view
    struct Worker {
        pid_t pid = -1;
        int request_fd = -1;
        int response_fd = -1;
        std::mutex write_mutex;
        std::thread reader;

        Worker() = default;
        Worker(const Worker&) = delete;
        Worker& operator=(const Worker&) = delete;
        Worker(Worker&&) = delete;
        Worker& operator=(Worker&&) = delete;
    };
private:
    // All worker processes owned by pool
    std::vector<std::unique_ptr<Worker>> workers_;

    // Protects worker selection / access to workers_ for Submit
    std::mutex workers_mutex_;

    // Round-robin index for selecting next worker
    std::size_t next_worker_ = 0;

    // Set to true once shutdown starts
    std::atomic<bool> stopping_{false};

    // Monotonically increasing unique task id
    std::atomic<std::uint64_t> next_task_id_{1};

    // Protects pending_ map
    std::mutex pending_mutex_;

    // Maps task id -> pending result handler
    std::unordered_map<std::uint64_t, std::unique_ptr<PendingBase>> pending_;
public:
    explicit ProcessPool(std::size_t process_count);
    ProcessPool(const ProcessPool&) = delete;
    ProcessPool& operator=(const ProcessPool&) = delete;
    ~ProcessPool();

    // Submit task to process pool and return Future for its result
    template <typename Fn, typename... Args>
    auto Submit(Fn fn, Args... args) -> Future<std::invoke_result_t<Fn, Args...>>;

    void Shutdown() noexcept;
private:
    // Create shared state object for Future<Result>
    template <typename Result>
    static auto MakeState();

    // Create correct pending handler
    template <typename Result>
    static std::unique_ptr<PendingBase> MakePending(const std::shared_ptr<detail::SharedState<Result>>& state);

    // Typed executor used inside worker process
    template <typename Fn, typename Result, typename... Args>
    static void ExecuteTyped(const std::byte* data, std::size_t size,
                             std::vector<std::byte>& output, std::string& error);

    // Create one worker process and return Worker descriptor in parent
    static std::unique_ptr<Worker> SpawnWorker();

    // Main loop executed in child process
    static void WorkerLoop(int request_fd, int response_fd) noexcept;

    // Main loop executed in parent thread for one worker
    void ReaderLoop(Worker& worker) noexcept;

    // Mark every unfinished future as failed
    void FailAllPending(const std::string& reason) noexcept;
};

template <typename Result>
auto ProcessPool::MakeState() {
    return std::make_shared<detail::SharedState<Result>>();
}

template <typename Result>
std::unique_ptr<ProcessPool::PendingBase>
ProcessPool::MakePending(const std::shared_ptr<detail::SharedState<Result>>& state) {
    if constexpr (std::is_void_v<Result>) {
        return std::make_unique<PendingVoid>(state);
    } else {
        return std::make_unique<PendingValue<Result>>(state);
    }
}

template <typename Fn, typename Result, typename... Args>
void ProcessPool::ExecuteTyped(const std::byte* data,
                               std::size_t size,
                               std::vector<std::byte>& output,
                               std::string& error) {
    try {
        std::size_t offset = 0;

        // First object in payload is always function pointer
        Fn fn = detail::ReadValue<Fn>(data, size, offset);

        // Then follow all function arguments in the same order
        auto args = std::tuple<std::decay_t<Args>...>{
            detail::ReadValue<std::decay_t<Args>>(data, size, offset)...
        };

        // If offset does not match payload size, then payload format is invalid
        if (offset != size) {
            throw std::runtime_error("payload has unexpected tail bytes");
        }

        if constexpr (std::is_void_v<Result>) {
            // Task returns no value, only execute it
            std::apply(fn, args);
        } else {
            // Execute function and serialize returned value into output bytes
            Result result = std::apply(fn, args);
            output.resize(sizeof(Result));
            std::memcpy(output.data(), &result, sizeof(Result));
        }
    } catch (const std::exception& e) {
        // Worker cannot transfer C++ exception object itself across process boundary, so it sends only textual message
        error = e.what();
    } catch (...) {
        error = "unknown exception";
    }
}

template <typename Fn, typename... Args>
auto ProcessPool::Submit(Fn fn, Args... args) -> Future<std::invoke_result_t<Fn, Args...>> {
    using Result = std::invoke_result_t<Fn, Args...>;
    using DecayedFn = std::decay_t<Fn>;

    // This is realistic for process-based execution because we transfer task
    // as raw bytes, not as arbitrary callable object with captured state
    static_assert(std::is_pointer_v<DecayedFn> &&
                      std::is_function_v<std::remove_pointer_t<DecayedFn>>,
                  "Submit supports only plain function pointers. Use unary plus for captureless lambdas.");

    // Arguments must be safe to copy byte-by-byte into payload
    static_assert(std::conjunction_v<std::is_trivially_copyable<std::decay_t<Args>>...>,
                  "All arguments must be trivially copyable.");

    // Result type must also be transferable as raw bytes
    static_assert(std::is_void_v<Result> || std::is_trivially_copyable_v<Result>,
                  "Return type must be void or trivially copyable.");

    // Create shared state for Future<Result>
    auto state = MakeState<Result>();

    // Generate unique task id.
    const std::uint64_t task_id = next_task_id_.fetch_add(1, std::memory_order_relaxed);

    // Save handler that will later complete this Future
    // when ReaderLoop receives response with the same task_id
    {
        std::lock_guard lock(pending_mutex_);
        pending_[task_id] = MakePending<Result>(state);
    }

    // Serialize function pointer and all arguments into one byte buffer
    std::vector<std::byte> payload;
    payload.reserve(sizeof(DecayedFn) + (sizeof(std::decay_t<Args>) + ... + 0));
    detail::AppendBytes(payload, fn);
    (detail::AppendBytes(payload, std::decay_t<Args>(args)), ...);

    // Build command header for worker process
    TaskHeader header{};
    header.command = Command::Run;
    header.task_id = task_id;

    // executor is pointer to a typed adapter function
    // Worker will call this function to decode payload and run actual task
    header.executor =
        reinterpret_cast<std::uintptr_t>(&ExecuteTyped<DecayedFn, Result, std::decay_t<Args>...>);
    header.payload_size = payload.size();

    Worker* worker = nullptr;
    {
        std::lock_guard lock(workers_mutex_);
        if (stopping_) {
            throw std::runtime_error("process pool is stopping");
        }

        // Select worker in round-robin order
        worker = workers_[next_worker_++ % workers_.size()].get();
    }

    // Send task header and payload to selected worker
    // write_mutex is required because multiple threads may call Submit()
    // simultaneously, and writes to the same socket must not interleave
    {
        std::lock_guard lock(worker->write_mutex);
        detail::WriteExact(worker->request_fd, &header, sizeof(header));
        if (!payload.empty()) {
            detail::WriteExact(worker->request_fd, payload.data(), payload.size());
        }
    }

    // Return Future connected to the created shared state
    return Future<Result>{std::move(state)};
}

}  // namespace processpool