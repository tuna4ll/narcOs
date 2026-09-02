#include <libc.h>

static long syscall2(long nr, long a0, long a1) {
    long ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(nr), "D"(a0), "S"(a1)
        : "memory"
    );
    return ret;
}

static long syscall1(long nr, long a0) {
    long ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(nr), "D"(a0)
        : "memory"
    );
    return ret;
}

long write(const void *buf, size_t len) {
    return syscall2(1, (long)buf, (long)len);
}

__attribute__((noreturn)) void exit(int status) {
    syscall1(60, status);
    for (;;) {}
}
