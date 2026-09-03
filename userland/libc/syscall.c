#include <libc.h>

static long syscall3(long nr, long a0, long a1, long a2) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(nr), "D"(a0), "S"(a1), "d"(a2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static long syscall1(long nr, long a0) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(nr), "D"(a0)
        : "rcx", "r11", "memory"
    );
    return ret;
}

long write(const void *buf, size_t len) {
    return syscall3(1, 1, (long)buf, (long)len);
}

__attribute__((noreturn)) void exit(int status) {
    syscall1(231, status);
    for (;;) {}
}
