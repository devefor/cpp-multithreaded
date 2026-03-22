#include "process_pool.h"

#include <sys/wait.h>
#include <unistd.h>

namespace processpool {

// Create pool with fixed number of worker processes
ProcessPool::ProcessPool(std::size_t process_count) {
    if (process_count == 0) {
        throw std::invalid_argument("process_count must be > 0");
    }

    workers_.reserve(process_count);
    for (std::size_t i = 0; i < process_count; ++i) {
        workers_.push_back(SpawnWorker());
    }

    for (auto& worker : workers_) {
        worker->reader = std::thread([this, w = worker.get()] {
            ReaderLoop(*w);
        });
    }
}

// Destructor performs graceful shutdown
ProcessPool::~ProcessPool() {
    Shutdown();
}

// Gracefully stop the whole process pool
void ProcessPool::Shutdown() noexcept {
    bool expected = false;
    if (!stopping_.compare_exchange_strong(expected, true)) {
        return;
    }

    for (auto& worker : workers_) {
        TaskHeader stop{};
        stop.command = Command::Stop;
        try {
            std::lock_guard lock(worker->write_mutex);
            detail::WriteExact(worker->request_fd, &stop, sizeof(stop));
        } catch (...) {
        }
    }

    for (auto& worker : workers_) {
        if (worker->reader.joinable()) {
            worker->reader.join();
        }
    }

    for (auto& worker : workers_) {
        if (worker->request_fd >= 0) {
            ::close(worker->request_fd);
            worker->request_fd = -1;
        }
        if (worker->response_fd >= 0) {
            ::close(worker->response_fd);
            worker->response_fd = -1;
        }
        if (worker->pid > 0) {
            int status = 0;
            while (::waitpid(worker->pid, &status, 0) < 0 && errno == EINTR) {
            }
            worker->pid = -1;
        }
    }

    FailAllPending("process pool shutdown");
}


// Create one worker process and communication channels for it
std::unique_ptr<ProcessPool::Worker> ProcessPool::SpawnWorker() {
    std::array<int, 2> request{};
    std::array<int, 2> response{};

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, request.data()) != 0) {
        detail::ThrowSystemError("socketpair(request) failed");
    }

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, response.data()) != 0) {
        ::close(request[0]);
        ::close(request[1]);
        detail::ThrowSystemError("socketpair(response) failed");
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(request[0]);
        ::close(request[1]);
        ::close(response[0]);
        ::close(response[1]);
        detail::ThrowSystemError("fork failed");
    }

    if (pid == 0) {
        ::close(request[0]);
        ::close(response[0]);
        WorkerLoop(request[1], response[1]);
        std::_Exit(0);
    }

    ::close(request[1]);
    ::close(response[1]);

    auto worker = std::make_unique<Worker>();
    worker->pid = pid;
    worker->request_fd = request[0];
    worker->response_fd = response[0];
    return worker;
}

// Main loop executed inside child process
void ProcessPool::WorkerLoop(int request_fd, int response_fd) noexcept {
    try {
        while (true) {
            TaskHeader header{};
            if (!detail::ReadExact(request_fd, &header, sizeof(header))) {
                break;
            }

            if (header.command == Command::Stop) {
                break;
            }

            std::vector<std::byte> payload(header.payload_size);
            if (!payload.empty() &&
                !detail::ReadExact(request_fd, payload.data(), payload.size())) {
                break;
            }

            auto executor = reinterpret_cast<ExecutorFn>(header.executor);
            std::vector<std::byte> result_payload;
            std::string error;
            executor(payload.data(), payload.size(), result_payload, error);

            ResultHeader response{};
            response.task_id = header.task_id;

            if (!error.empty()) {
                response.status = ResultStatus::Exception;
                response.payload_size = error.size();
                detail::WriteExact(response_fd, &response, sizeof(response));
                detail::WriteExact(response_fd, error.data(), error.size());
            } else if (result_payload.empty()) {
                response.status = ResultStatus::Void;
                response.payload_size = 0;
                detail::WriteExact(response_fd, &response, sizeof(response));
            } else {
                response.status = ResultStatus::Value;
                response.payload_size = result_payload.size();
                detail::WriteExact(response_fd, &response, sizeof(response));
                detail::WriteExact(response_fd, result_payload.data(), result_payload.size());
            }
        }
    } catch (...) {
    }

    ::close(request_fd);
    ::close(response_fd);
}

// Main loop executed in parent-side reader thread for one worker
void ProcessPool::ReaderLoop(Worker& worker) noexcept {
    try {
        while (true) {
            ResultHeader header{};
            if (!detail::ReadExact(worker.response_fd, &header, sizeof(header))) {
                break;
            }

            std::vector<std::byte> payload(header.payload_size);
            if (!payload.empty() &&
                !detail::ReadExact(worker.response_fd, payload.data(), payload.size())) {
                break;
            }

            std::unique_ptr<PendingBase> pending;
            {
                std::lock_guard lock(pending_mutex_);
                auto it = pending_.find(header.task_id);
                if (it == pending_.end()) {
                    continue;
                }
                pending = std::move(it->second);
                pending_.erase(it);
            }

            if (header.status == ResultStatus::Exception) {
                pending->SetException(
                    std::string(reinterpret_cast<const char*>(payload.data()), payload.size()));
            } else {
                pending->SetValue(payload);
            }
        }
    } catch (...) {
    }
}

// Fail all unfinished tasks
void ProcessPool::FailAllPending(const std::string& reason) noexcept {
    std::unordered_map<std::uint64_t, std::unique_ptr<PendingBase>> temp;
    {
        std::lock_guard lock(pending_mutex_);
        temp.swap(pending_);
    }

    for (auto& [id, pending] : temp) {
        (void)id;
        pending->SetException(reason);
    }
}

}  // namespace processpool