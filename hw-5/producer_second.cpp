#include "mpsc_queue.h"

#include <iostream>

using namespace mpsc_queue;

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <shm_path> <shm_size>\n";
        return 1;
    }

    const char* shmPath = argv[1];
    const uint64_t shmSize = std::stoull(argv[2]);

    ProducerNode producer(shmPath, shmSize);

    std::cout << "[PRODUCER_2]: connected to queue\n";

    producer.SendText("hello from producer 2");
    producer.SendNumber(222);
    producer.SendText("another message from producer 2");

    std::cout << "[PRODUCER_2]: done\n";
    return 0;
}