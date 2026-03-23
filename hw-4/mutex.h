#pragma once

#include <atomic>
#include <cstdint>

// Atomically do the following:
//    if (*(uint64_t*)addr == expected_value) {
//        sleep_on_address(addr)
//    }
void FutexWait(void *addr, uint64_t expected_value);

// Wakeup 1 thread sleeping on the given address
void FutexWakeOne(void *addr);

// Wakeup all threads sleeping on the given address
void FutexWakeAll(void *addr);

class Mutex {
private:
   std::atomic<uint32_t> mtx_state{0};
public:
    void Lock() {
        uint32_t expected = 0;
        // state = 0 -> 1
        if (!mtx_state.compare_exchange_strong(expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
            if (expected != 2) {
                // state = 1 -> 2
                expected = mtx_state.exchange(2, std::memory_order_acquire);
            }

            while (expected != 0) {
                // wait unlock
                FutexWait(static_cast<void*>(&mtx_state), 2);
                expected = mtx_state.exchange(2, std::memory_order_acquire);
            }
        }
    }

    // Any state -> 0
    void Unlock() {
        const uint32_t prev = mtx_state.exchange(0, std::memory_order_release);
        if (prev == 2) {
            // wake up signal to one
            FutexWakeOne(static_cast<void*>(&mtx_state));
        } 
    }
};
// state = 0 (Free)
// state = 1 (Captured without competition)
// state = 2 (Captured, there is competition)
