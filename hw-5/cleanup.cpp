#include "mpsc_queue.h"

#include <iostream>

using namespace mpsc_queue;

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <shm_path>\n";
        return 1;
    }

    CleanupSharedMemory(argv[1]);
    std::cout << "[CLEANUP]: shared memory removed\n";
    return 0;
}