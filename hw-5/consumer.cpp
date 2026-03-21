#include "mpsc_queue.h"

#include <iostream>
#include <stdexcept>
#include <string>

using namespace mpsc_queue;

MessageType ParseMessageType(const std::string& value) {
    if (value == "text") {
        return MessageType::Text;
    }

    if (value == "number") {
        return MessageType::Number;
    }

    if (value == "stop") {
        return MessageType::Stop;
    }

    if (value == "all") {
        return MessageType::All;
    }

    throw std::runtime_error("Unknown message type. Use: text | number | stop | all");
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <shm_path> <shm_size> <text|number|stop|all>\n";
        return 1;
    }

    const char* shmPath = argv[1];
    const uint64_t shmSize = std::stoull(argv[2]);
    const MessageType interestedType = ParseMessageType(argv[3]);

    ConsumerNode consumer(shmPath, shmSize, interestedType);

    std::cout << "[CONSUMER]: opened queue\n";

    for (;;) {
        ReceiveMessage message = consumer.Receive();

        if (message.type == MessageType::Stop) {
            std::cout << "[CONSUMER]: received Stop, exiting\n";
            break;
        }

        if (message.type == MessageType::Text) {
            std::cout << "[CONSUMER]: Text(\"" << BytesToString(message.payload) << "\")\n";
            continue;
        }

        if (message.type == MessageType::Number) {
            std::cout << "[CONSUMER]: Number(" << BytesToInt64(message.payload) << ")\n";
            continue;
        }
    }

    return 0;
}