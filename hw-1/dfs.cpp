#include "dfs.h"

#include <coroutine>
#include <deque>
#include <exception>
#include <ostream>
#include <stdexcept>
#include <utility>
#include <vector>

class Scheduler;

class Task {
public:
    struct promise_type {
        std::exception_ptr exception;
        // Create coroutine frame
        Task get_return_object() {
            return Task{
                std::coroutine_handle<promise_type>::from_promise(*this)
            };
        }

        // Create -> Stop
        std::suspend_always initial_suspend() noexcept {
            return {};
        }

        // End -> Stop
        std::suspend_always final_suspend() noexcept {
            return {};
        }

        void return_void() noexcept {}

        void unhandled_exception() {
            exception = std::current_exception();
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit Task(handle_type handle = nullptr) noexcept
        : handle_(handle) {
    }

    Task(Task&& other) noexcept
        : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    ~Task() = default;

    [[nodiscard]] handle_type Release() noexcept {
        handle_type tmp = handle_;
        handle_ = nullptr;
        return tmp;
    }
private:
    handle_type handle_;
};

class Scheduler {
public:
    Scheduler() {
        current_ = this;
    }

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    ~Scheduler() {
        while (!ready_queue_.empty()) {
            auto h = ready_queue_.front();
            ready_queue_.pop_front();
            if (h) {
                h.destroy();
            }
        }
        current_ = nullptr;
    }

    void Spawn(Task task) {
        auto handle = task.Release();
        if (handle) {
            ready_queue_.push_back(handle);
        }
    }

    void Schedule(std::coroutine_handle<> handle) {
        ready_queue_.push_back(handle);
    }

    void Run() {
        while (!ready_queue_.empty()) {
            auto handle = ready_queue_.front();
            ready_queue_.pop_front();

            if (!handle.done()) {
                handle.resume();
            }

            auto typed_handle =
                std::coroutine_handle<Task::promise_type>::from_address(handle.address());

            if (typed_handle.promise().exception) {
                std::exception_ptr ex = typed_handle.promise().exception;
                handle.destroy();
                std::rethrow_exception(ex);
            }

            if (handle.done()) {
                handle.destroy();
            }
        }
    }

    struct YieldAwaiter {
        bool await_ready() const noexcept {
            return false;
        }

        void await_suspend(std::coroutine_handle<> handle) const {
            Scheduler::Current().Schedule(handle);
        }

        void await_resume() const noexcept {}
    };

    static YieldAwaiter Yield() noexcept {
        return {};
    }

    static Scheduler& Current() {
        if (current_ == nullptr) {
            throw std::runtime_error("Scheduler is not initialized");
        }
        return *current_;
    }

private:
    std::deque<std::coroutine_handle<>> ready_queue_;
    inline static thread_local Scheduler* current_ = nullptr;
};

Task DfsCoroutine(
    const Graph& graph,
    int start_vertex,
    std::vector<bool>& visited,
    std::vector<int>& traversal_order,
    std::ostream* log
) {
    std::vector<std::pair<int, std::size_t>> stack;
    stack.emplace_back(start_vertex, 0);

    visited[start_vertex] = true;
    traversal_order.push_back(start_vertex);

    if (log != nullptr) *log << "enter vertex " << start_vertex << '\n';
    co_await Scheduler::Yield();

    while (!stack.empty()) {
        auto& [vertex, next_edge_index] = stack.back();

        if (next_edge_index >= graph.adj[vertex].size()) {
            if (log != nullptr) *log << "leave vertex " << vertex << '\n';
            stack.pop_back();
            co_await Scheduler::Yield();
            continue;
        }

        int to = graph.adj[vertex][next_edge_index];
        ++next_edge_index;

        if (to < 0 || to >= static_cast<int>(graph.adj.size())) {
            throw std::out_of_range("Graph contains invalid adjacent vertex index");
        }

        if (log != nullptr) *log << "inspect edge " << vertex << " -> " << to << '\n';
        co_await Scheduler::Yield();

        if (!visited[to]) {
            visited[to] = true;
            traversal_order.push_back(to);
            if (log != nullptr) *log << "enter vertex " << to << '\n';
            stack.emplace_back(to, 0);
            co_await Scheduler::Yield();
        }
    }
}

std::vector<int> CooperativeDFS(const Graph& graph, std::ostream* log) {
    Scheduler scheduler;

    std::vector<bool> visited(graph.adj.size(), false);
    std::vector<int> traversal_order;
    traversal_order.reserve(graph.adj.size());

    for (int i = 0; i < static_cast<int>(graph.adj.size()); ++i) {
        if (!visited[i]) {
            scheduler.Spawn(DfsCoroutine(graph, i, visited, traversal_order, log));
        }
    }

    scheduler.Run();
    return traversal_order;
}
