#include <mutex.h>
#include <cstdint>

/* Implementation of futex syscalls for Linux & MacOS */

#if defined(__linux__)

#include <unistd.h>
#include <linux/futex.h>
#include <sys/syscall.h>

void FutexWait(void *addr, uint64_t expected_value) {
    syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, expected_value, nullptr, nullptr, 0);
}

void FutexWakeOne(void *addr) {
    syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0);
}

void FutexWakeAll(void *addr) {
    syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, INT_MAX, nullptr, nullptr, 0);
}

#endif
