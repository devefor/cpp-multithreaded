#include "mpsc_queue.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>
#include <thread>

namespace mpsc_queue {

namespace {
constexpr uint64_t HeaderSize = sizeof(SharedMemoryLayout);

uint64_t GetBufferSizeFromShmSize(uint64_t shmSize) {
    if (shmSize <= HeaderSize) {
        throw std::runtime_error("Shared memory size is too small");
    }
    return shmSize - HeaderSize;
}

uint8_t* GetBufferBegin(void* memory) {
    return static_cast<uint8_t*>(memory) + sizeof(SharedMemoryLayout);
}

const uint8_t* GetBufferBegin(const void* memory) {
    return static_cast<const uint8_t*>(memory) + sizeof(SharedMemoryLayout);
}
} // namespace

uint32_t AlignUp(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

SharedMemoryRegion::SharedMemoryRegion(const char* path, uint64_t shmSize, SharedMemoryOpenMode mode)
    : sharedMemoryPath(path), sharedMemorySize(shmSize) {
    if (mode == SharedMemoryOpenMode::OpenOrCreate) {
        sharedMemoryFd = shm_open(path, O_CREAT | O_EXCL | O_RDWR, 0666);
        if (sharedMemoryFd != -1) {
            creator = true;
            CHECK_ERROR(ftruncate(sharedMemoryFd, static_cast<off_t>(shmSize)));
        } else if (errno == EEXIST) {
            sharedMemoryFd = shm_open(path, O_RDWR, 0666);
            CHECK_ERROR(sharedMemoryFd);
        } else {
            CHECK_ERROR(sharedMemoryFd);
        }
    } else {
        sharedMemoryFd = shm_open(path, O_RDWR, 0666);
        CHECK_ERROR(sharedMemoryFd);
    }

    sharedMemory = mmap(nullptr, shmSize, PROT_READ | PROT_WRITE, MAP_SHARED, sharedMemoryFd, 0);
    if (sharedMemory == MAP_FAILED) {
        throw std::runtime_error("mmap failed");
    }
}

SharedMemoryRegion::~SharedMemoryRegion() {
    if (sharedMemory && sharedMemory != MAP_FAILED) {
        CHECK_ERROR(munmap(sharedMemory, sharedMemorySize));
    }

    if (sharedMemoryFd != -1) {
        CHECK_ERROR(close(sharedMemoryFd));
    }
}

void* SharedMemoryRegion::GetMemory() {
    return sharedMemory;
}

const void* SharedMemoryRegion::GetMemory() const {
    return sharedMemory;
}

uint64_t SharedMemoryRegion::GetSize() const {
    return sharedMemorySize;
}

bool SharedMemoryRegion::IsCreator() const {
    return creator;
}

void SharedMemoryRegion::Unlink() const {
    if (shm_unlink(sharedMemoryPath) == -1 && errno != ENOENT) {
        CHECK_ERROR(-1);
    }
}

ProducerNode::ProducerNode(const char* path, uint64_t shmSize)
    : shmRegion(path, shmSize, SharedMemoryOpenMode::OpenOrCreate),
      shmLayout(static_cast<SharedMemoryLayout*>(shmRegion.GetMemory())),
      buffer(GetBufferBegin(shmRegion.GetMemory())),
      bufferSize(GetBufferSizeFromShmSize(shmSize)) {
    InitializeIfNeeded(shmSize);

    shmLayout->protocol.producerCount.fetch_add(1, std::memory_order_acq_rel);
    shmLayout->protocol.refCount.fetch_add(1, std::memory_order_acq_rel);
}

ProducerNode::~ProducerNode() {
    if (shmLayout != nullptr) {
        shmLayout->protocol.producerCount.fetch_sub(1, std::memory_order_acq_rel);
        shmLayout->protocol.refCount.fetch_sub(1, std::memory_order_acq_rel);
    }
}

void ProducerNode::InitializeIfNeeded(uint64_t shmSize) {
    if (shmRegion.IsCreator()) {
        new (shmRegion.GetMemory()) SharedMemoryLayout{};

        shmLayout->protocol.initState.store(
            static_cast<uint32_t>(InitState::Initializing),
            std::memory_order_relaxed);

        shmLayout->protocol.magic = ProtocolMagic;
        shmLayout->protocol.version = ProtocolVersion;
        shmLayout->protocol.shmSize = shmSize;
        shmLayout->protocol.bufferSize = bufferSize;
        shmLayout->protocol.producerCount.store(0, std::memory_order_relaxed);
        shmLayout->protocol.consumerCount.store(0, std::memory_order_relaxed);
        shmLayout->protocol.refCount.store(0, std::memory_order_relaxed);

        shmLayout->queue.reservePos.store(0, std::memory_order_relaxed);
        shmLayout->queue.commitPos.store(0, std::memory_order_relaxed);
        shmLayout->queue.readPos.store(0, std::memory_order_relaxed);

        shmLayout->protocol.initState.store(
            static_cast<uint32_t>(InitState::Ready),
            std::memory_order_release);
        return;
    }

    WaitUntilReady();
    ValidateProtocol(shmSize);
}

void ProducerNode::WaitUntilReady() const {
    while (static_cast<InitState>(
               shmLayout->protocol.initState.load(std::memory_order_acquire)) != InitState::Ready) {
        std::this_thread::yield();
    }
}

void ProducerNode::ValidateProtocol(uint64_t shmSize) const {
    if (shmLayout->protocol.magic != ProtocolMagic) {
        throw std::runtime_error("Invalid protocol magic");
    }

    if (shmLayout->protocol.version != ProtocolVersion) {
        throw std::runtime_error("Unsupported protocol version");
    }

    if (shmLayout->protocol.shmSize != shmSize) {
        throw std::runtime_error("Shared memory size mismatch");
    }

    if (shmLayout->protocol.bufferSize != bufferSize) {
        throw std::runtime_error("Shared memory buffer size mismatch");
    }

    if (static_cast<InitState>(
            shmLayout->protocol.initState.load(std::memory_order_acquire)) != InitState::Ready) {
        throw std::runtime_error("Shared memory is not ready");
    }
}

bool ProducerNode::TrySend(MessageType type, std::span<const std::byte> payload) {
    const uint32_t payloadSize = static_cast<uint32_t>(payload.size());
    const uint32_t messageSize = AlignUp(sizeof(MessageHeader) + payloadSize);

    if (messageSize > bufferSize) {
        throw std::runtime_error("Message is too large for ring buffer");
    }

    for (;;) {
        const uint64_t readPos = shmLayout->queue.readPos.load(std::memory_order_acquire);
        const uint64_t reservePos = shmLayout->queue.reservePos.load(std::memory_order_acquire);

        const uint64_t used = reservePos - readPos;
        const uint64_t freeSpace = bufferSize - used;

        const uint64_t offset = reservePos % bufferSize;
        const uint64_t tailSpace = bufferSize - offset;

        uint32_t paddingSize = 0;
        if (tailSpace < messageSize) {
            paddingSize = static_cast<uint32_t>(tailSpace);
        }

        const uint64_t required = static_cast<uint64_t>(paddingSize) + messageSize;
        if (freeSpace < required) {
            return false;
        }

        const uint64_t newReservePos = reservePos + required;
        uint64_t expected = reservePos;

        if (!shmLayout->queue.reservePos.compare_exchange_weak(
                expected,
                newReservePos,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            continue;
        }

        if (paddingSize >= sizeof(MessageHeader)) {
            WritePaddingRecord(offset, paddingSize);
        }

        const uint64_t messageAbsolutePos = reservePos + paddingSize;
        const uint64_t messageOffset = messageAbsolutePos % bufferSize;
        WriteMessageRecord(messageOffset, type, payload);

        PublishReservedRange(reservePos, newReservePos);
        return true;
    }
}

void ProducerNode::Send(MessageType type, std::span<const std::byte> payload) {
    while (!TrySend(type, payload)) {
        std::this_thread::yield();
    }
}

void ProducerNode::SendText(std::string_view text) {
    const auto* ptr = reinterpret_cast<const std::byte*>(text.data());
    Send(MessageType::Text, std::span<const std::byte>{ptr, text.size()});
}

void ProducerNode::SendNumber(int64_t value) {
    const auto* ptr = reinterpret_cast<const std::byte*>(&value);
    Send(MessageType::Number, std::span<const std::byte>{ptr, sizeof(value)});
}

void ProducerNode::SendStop() {
    Send(MessageType::Stop, {});
}

void ProducerNode::WritePaddingRecord(uint64_t offset, uint32_t paddingSize) {
    MessageHeader header{
        .type = static_cast<uint32_t>(MessageType::Padding),
        .payloadSize = paddingSize - static_cast<uint32_t>(sizeof(MessageHeader)),
    };

    std::memcpy(buffer + offset, &header, sizeof(header));

    if (paddingSize > sizeof(MessageHeader)) {
        std::memset(buffer + offset + sizeof(MessageHeader), 0, paddingSize - sizeof(MessageHeader));
    }
}

void ProducerNode::WriteMessageRecord(uint64_t offset, MessageType type, std::span<const std::byte> payload) {
    MessageHeader header{
        .type = static_cast<uint32_t>(type),
        .payloadSize = static_cast<uint32_t>(payload.size()),
    };

    std::memcpy(buffer + offset, &header, sizeof(header));

    if (!payload.empty()) {
        std::memcpy(buffer + offset + sizeof(header), payload.data(), payload.size());
    }

    const uint32_t totalSize = AlignUp(sizeof(MessageHeader) + header.payloadSize);
    const uint32_t paddingBytes = totalSize - sizeof(MessageHeader) - header.payloadSize;
    if (paddingBytes != 0) {
        std::memset(buffer + offset + sizeof(header) + header.payloadSize, 0, paddingBytes);
    }
}

void ProducerNode::PublishReservedRange(uint64_t myBegin, uint64_t myEnd) {
    for (;;) {
        uint64_t expected = myBegin;
        if (shmLayout->queue.commitPos.compare_exchange_weak(
                expected, myEnd,
                std::memory_order_release,
                std::memory_order_acquire)) {
            return;
        }

        std::this_thread::yield();
    }
}

ConsumerNode::ConsumerNode(const char* path, uint64_t shmSize, MessageType interestedType)
    : shmRegion(path, shmSize, SharedMemoryOpenMode::OpenExisting),
      shmLayout(static_cast<SharedMemoryLayout*>(shmRegion.GetMemory())),
      buffer(GetBufferBegin(shmRegion.GetMemory())),
      bufferSize(GetBufferSizeFromShmSize(shmSize)),
      interestedType(interestedType) {
    WaitUntilReady();
    ValidateProtocol(shmSize);

    shmLayout->protocol.consumerCount.fetch_add(1, std::memory_order_acq_rel);
    shmLayout->protocol.refCount.fetch_add(1, std::memory_order_acq_rel);
}

ConsumerNode::~ConsumerNode() {
    if (shmLayout != nullptr) {
        shmLayout->protocol.consumerCount.fetch_sub(1, std::memory_order_acq_rel);
        const uint32_t prevRefCount =
            shmLayout->protocol.refCount.fetch_sub(1, std::memory_order_acq_rel);

        if (prevRefCount == 1) {
            shmRegion.Unlink();
        }
    }
}

void ConsumerNode::WaitUntilReady() const {
    while (static_cast<InitState>(
               shmLayout->protocol.initState.load(std::memory_order_acquire)) != InitState::Ready) {
        std::this_thread::yield();
    }
}

void ConsumerNode::ValidateProtocol(uint64_t shmSize) const {
    if (shmLayout->protocol.magic != ProtocolMagic) {
        throw std::runtime_error("Invalid protocol magic");
    }

    if (shmLayout->protocol.version != ProtocolVersion) {
        throw std::runtime_error("Unsupported protocol version");
    }

    if (shmLayout->protocol.shmSize != shmSize) {
        throw std::runtime_error("Shared memory size mismatch");
    }

    if (shmLayout->protocol.bufferSize != bufferSize) {
        throw std::runtime_error("Shared memory buffer size mismatch");
    }
}

std::optional<ReceiveMessage> ConsumerNode::TryReceive() {
    for (;;) {
        uint64_t readPos = shmLayout->queue.readPos.load(std::memory_order_acquire);
        const uint64_t commitPos = shmLayout->queue.commitPos.load(std::memory_order_acquire);

        if (readPos == commitPos) {
            return std::nullopt;
        }

        uint64_t offset = readPos % bufferSize;
        const uint64_t tailSpace = bufferSize - offset;

        if (tailSpace < sizeof(MessageHeader)) {
            shmLayout->queue.readPos.store(readPos + tailSpace, std::memory_order_release);
            continue;
        }

        MessageHeader header{};
        std::memcpy(&header, buffer + offset, sizeof(header));

        const MessageType type = static_cast<MessageType>(header.type);
        const uint32_t totalSize = AlignUp(sizeof(MessageHeader) + header.payloadSize);
        const uint64_t nextReadPos = readPos + totalSize;

        if (nextReadPos > commitPos) {
            return std::nullopt;
        }

        if (type == MessageType::Padding) {
            shmLayout->queue.readPos.store(nextReadPos, std::memory_order_release);
            continue;
        }

        std::optional<ReceiveMessage> result;

        if (interestedType == MessageType::All || type == interestedType || type == MessageType::Stop) {
            ReceiveMessage message;
            message.type = type;
            message.payload.resize(header.payloadSize);

            if (header.payloadSize != 0) {
                std::memcpy(message.payload.data(),
                            buffer + offset + sizeof(header),
                            header.payloadSize);
            }

            result = std::move(message);
        }

        shmLayout->queue.readPos.store(nextReadPos, std::memory_order_release);
        return result;
    }
}

ReceiveMessage ConsumerNode::Receive() {
    for (;;) {
        auto message = TryReceive();
        if (message) {
            return *message;
        }

        std::this_thread::yield();
    }
}

std::string BytesToString(const std::vector<std::byte>& bytes) {
    return {
        reinterpret_cast<const char*>(bytes.data()),
        reinterpret_cast<const char*>(bytes.data()) + bytes.size()
    };
}

int64_t BytesToInt64(const std::vector<std::byte>& bytes) {
    if (bytes.size() != sizeof(int64_t)) {
        throw std::runtime_error("Invalid int64 payload size");
    }

    int64_t value{};
    std::memcpy(&value, bytes.data(), sizeof(value));
    return value;
}
} // namespace mpsc_queue