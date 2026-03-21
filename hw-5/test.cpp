#include "mpsc_queue.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <sys/mman.h>
#include <thread>

using namespace mpsc_queue;

namespace {
    constexpr const char* kTestShmPath = "/mpsc_queue_test_shm";
    constexpr uint64_t kTestShmSize = 1 << 16;

    void RemoveSharedMemoryIfExists(const char* path) {
        if (shm_unlink(path) == -1 && errno != ENOENT) {
            FAIL() << "shm_unlink failed for path " << path;
        }
    }
}

class QueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        RemoveSharedMemoryIfExists(kTestShmPath);
    }

    void TearDown() override {
        RemoveSharedMemoryIfExists(kTestShmPath);
    }
};

TEST_F(QueueTest, SingleProducerSingleConsumerText) {
    ProducerNode producer(kTestShmPath, kTestShmSize);
    ConsumerNode consumer(kTestShmPath, kTestShmSize, MessageType::Text);

    producer.SendText("hello");

    ReceiveMessage message = consumer.Receive();

    ASSERT_EQ(message.type, MessageType::Text);
    ASSERT_EQ(BytesToString(message.payload), "hello");
}

TEST_F(QueueTest, ConsumerFiltersOtherTypes) {
    ProducerNode producer(kTestShmPath, kTestShmSize);
    ConsumerNode consumer(kTestShmPath, kTestShmSize, MessageType::Text);

    producer.SendNumber(42);
    producer.SendText("wanted");

    ReceiveMessage message = consumer.Receive();

    ASSERT_EQ(message.type, MessageType::Text);
    ASSERT_EQ(BytesToString(message.payload), "wanted");
}

TEST_F(QueueTest, TwoProducersWriteIntoSameQueue) {
    ProducerNode producer1(kTestShmPath, kTestShmSize);
    ProducerNode producer2(kTestShmPath, kTestShmSize);
    ConsumerNode consumer(kTestShmPath, kTestShmSize, MessageType::Text);

    std::thread t1([&]() {
        producer1.SendText("from producer 1");
    });

    std::thread t2([&]() {
        producer2.SendText("from producer 2");
    });

    t1.join();
    t2.join();

    ReceiveMessage first = consumer.Receive();
    ReceiveMessage second = consumer.Receive();

    const std::string s1 = BytesToString(first.payload);
    const std::string s2 = BytesToString(second.payload);

    ASSERT_TRUE(
        (s1 == "from producer 1" && s2 == "from producer 2") ||
        (s1 == "from producer 2" && s2 == "from producer 1"));
}

TEST_F(QueueTest, ProducerCountTracksConnectedProducers) {
    ProducerNode producer1(kTestShmPath, kTestShmSize);
    ProducerNode producer2(kTestShmPath, kTestShmSize);

    SharedMemoryRegion region(kTestShmPath, kTestShmSize, SharedMemoryOpenMode::OpenExisting);
    auto* layout = static_cast<SharedMemoryLayout*>(region.GetMemory());

    ASSERT_EQ(layout->protocol.producerCount.load(std::memory_order_acquire), 2u);
}