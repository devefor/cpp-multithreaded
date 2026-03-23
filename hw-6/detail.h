#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <errno.h>
#include <exception>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <sys/types.h>
#include <unistd.h>

namespace processpool::detail {

// Throws std::runtime_error with the current errno text appended.
inline void ThrowSystemError(const std::string& message) {
    throw std::runtime_error(message + ": " + std::strerror(errno));
}

// Writes exactly size bytes into file descriptor fd
inline void WriteExact(int fd, const void* data, std::size_t size) {
    const auto* ptr = static_cast<const std::byte*>(data);
    std::size_t written = 0;
    
    // Why a loop - one call to write() is not guaranteed to write the whole buffer
    while (written < size) {
        const ssize_t rc = ::write(fd, ptr + written, size - written);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            ThrowSystemError("write failed");
        }
        if (rc == 0) {
            throw std::runtime_error("write returned 0");
        }
        written += static_cast<std::size_t>(rc);
    }
}


// Reads exactly size bytes from file descriptor fd
// Returns:
// - true  -> requested number of bytes was read successfully
// - false -> read() returned 0, meaning EOF / peer closed connection
inline bool ReadExact(int fd, void* data, std::size_t size) {
    auto* ptr = static_cast<std::byte*>(data);
    std::size_t read_bytes = 0;

    // Why a loop - one call to read() may return fewer bytes than requested
    while (read_bytes < size) {
        const ssize_t rc = ::read(fd, ptr + read_bytes, size - read_bytes);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            ThrowSystemError("read failed");
        }
        if (rc == 0) {
            return false;
        }
        read_bytes += static_cast<std::size_t>(rc);
    }
    return true;
}

// Shared state between producer of result and Future<T>
// This is an analogue of the internal shared state used by
// std::promise/std::future
template <typename T>
struct SharedState {
    mutable std::mutex mutex;
    std::condition_variable cv;
    bool ready = false;
    std::optional<T> value;
    std::exception_ptr error;
};

// Specialization for void result
template <>
struct SharedState<void> {
    mutable std::mutex mutex;
    std::condition_variable cv;
    bool ready = false;
    std::exception_ptr error;
};

// Appends raw bytes of a trivially copyable object to a byte buffer
// Examples of suitable types:
// - int, double, POD structs, plain function pointers
// Examples of unsuitable types:
// - std::string, std::vector, classes with dynamic memory
template <typename T>
inline void AppendBytes(std::vector<std::byte>& buffer, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    const auto* ptr = reinterpret_cast<const std::byte*>(&value);
    buffer.insert(buffer.end(), ptr, ptr + sizeof(T));
}

// Reads one object of type T from a raw byte array starting at offset
template <typename T>
inline T ReadValue(const std::byte* data, std::size_t size, std::size_t& offset) {
    static_assert(std::is_trivially_copyable_v<T>);
    if (offset + sizeof(T) > size) {
        throw std::runtime_error("invalid serialized payload");
    }

    T value{};
    std::memcpy(&value, data + offset, sizeof(T));
    offset += sizeof(T);
    return value;
}

}  // namespace processpool::detail