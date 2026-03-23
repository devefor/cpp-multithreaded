#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <source_location>

namespace mpsc_queue {

static constexpr uint32_t ProtocolMagic = 0x4D505343;
static constexpr uint32_t ProtocolVersion = 1;
static constexpr uint32_t Alignment = 8;

enum class InitState : uint32_t {
    Uninitialized = 0,
    Initializing = 1,
    Ready = 2,
};

enum class MessageType : uint32_t {
    Padding = 0,
    Text = 1,
    Number = 2,
    Stop = 3,
    All = 4,
};

uint32_t AlignUp(uint32_t value, uint32_t alignment = Alignment);

struct ProtocolHeader {
    uint32_t magic{ProtocolMagic};
    uint32_t version{ProtocolVersion};
    uint64_t shmSize{};
    uint64_t bufferSize{};

    std::atomic<uint32_t> initState{static_cast<uint32_t>(InitState::Uninitialized)};
    std::atomic<uint32_t> producerCount{0};
    std::atomic<uint32_t> consumerCount{0};
    std::atomic<uint32_t> refCount{0};
};

struct alignas(64) QueueHeader {
    std::atomic<uint64_t> reservePos{0};
    std::atomic<uint64_t> commitPos{0};
    std::atomic<uint64_t> readPos{0};
};

struct SharedMemoryLayout {
    ProtocolHeader protocol{};
    QueueHeader queue{};
};

struct MessageHeader {
    uint32_t type{};
    uint32_t payloadSize{};
};

struct ReceiveMessage {
    MessageType type{};
    std::vector<std::byte> payload;
};

enum class SharedMemoryOpenMode {
    OpenOrCreate,
    OpenExisting,
};

class SharedMemoryRegion {
private:
    const char* sharedMemoryPath{};
    uint64_t sharedMemorySize{};
    int sharedMemoryFd{-1};
    void* sharedMemory{};
    bool creator{false};

public:
    SharedMemoryRegion(const char* path, uint64_t shmSize, SharedMemoryOpenMode mode);
    ~SharedMemoryRegion();

    SharedMemoryRegion(const SharedMemoryRegion&) = delete;
    SharedMemoryRegion& operator=(const SharedMemoryRegion&) = delete;
    SharedMemoryRegion(SharedMemoryRegion&&) = delete;
    SharedMemoryRegion& operator=(SharedMemoryRegion&&) = delete;

    void* GetMemory();
    const void* GetMemory() const;
    uint64_t GetSize() const;
    bool IsCreator() const;
    void Unlink() const;
};

class ProducerNode {
private:
    SharedMemoryRegion shmRegion;
    SharedMemoryLayout* shmLayout{};
    uint8_t* buffer{};
    uint64_t bufferSize{};

public:
    ProducerNode(const char* path, uint64_t shmSize);
    ~ProducerNode();

    ProducerNode(const ProducerNode&) = delete;
    ProducerNode& operator=(const ProducerNode&) = delete;
    ProducerNode(ProducerNode&&) = delete;
    ProducerNode& operator=(ProducerNode&&) = delete;

    bool TrySend(MessageType type, std::span<const std::byte> payload);
    void Send(MessageType type, std::span<const std::byte> payload);

    void SendText(std::string_view text);
    void SendNumber(int64_t value);
    void SendStop();

private:
    void InitializeIfNeeded(uint64_t shmSize);
    void WaitUntilReady() const;
    void ValidateProtocol(uint64_t shmSize) const;
    void WritePaddingRecord(uint64_t offset, uint32_t paddingSize);
    void WriteMessageRecord(uint64_t offset, MessageType type, std::span<const std::byte> payload);
    void PublishReservedRange(uint64_t myBegin, uint64_t myEnd);
};

class ConsumerNode {
private:
    SharedMemoryRegion shmRegion;
    SharedMemoryLayout* shmLayout{};
    uint8_t* buffer{};
    uint64_t bufferSize{};
    MessageType interestedType{};

public:
    ConsumerNode(const char* path, uint64_t shmSize, MessageType interestedType);
    ~ConsumerNode();

    ConsumerNode(const ConsumerNode&) = delete;
    ConsumerNode& operator=(const ConsumerNode&) = delete;
    ConsumerNode(ConsumerNode&&) = delete;
    ConsumerNode& operator=(ConsumerNode&&) = delete;

    std::optional<ReceiveMessage> TryReceive();
    ReceiveMessage Receive();

private:
    void WaitUntilReady() const;
    void ValidateProtocol(uint64_t shmSize) const;
};

std::string BytesToString(const std::vector<std::byte>& bytes);
int64_t BytesToInt64(const std::vector<std::byte>& bytes);

} // namespace mpsc_queue

inline void LogSystemError(std::source_location loc = std::source_location::current()) {
    std::cerr << "Error in " << loc.function_name() << "\n"
              << "  File: " << loc.file_name() << ":" << loc.line() << "\n"
              << "  Reason: " << std::strerror(errno) << " (errno: " << errno << ")\n";
}

#define CHECK_ERROR(res)                    \
    do {                                    \
        if ((res) == -1) {                  \
            LogSystemError();               \
            std::exit(EXIT_FAILURE);        \
        }                                   \
    } while (0)