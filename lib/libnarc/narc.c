#include <narcos/narc.h>

static narc_result_t narc_call6(uint64_t id, uint64_t a1, uint64_t a2,
                                uint64_t a3, uint64_t a4, uint64_t a5,
                                uint64_t a6) {
    uint64_t number = NARC_SYSCALL_TAG | id;
    uint64_t status;

#if defined(__x86_64__)
    register uint64_t arg4 __asm__("r10") = a4;
    register uint64_t arg5 __asm__("r8") = a5;
    register uint64_t arg6 __asm__("r9") = a6;
    status = a3;
    __asm__ volatile (
        "syscall"
        : "+a"(number), "+d"(status)
        : "D"(a1), "S"(a2), "r"(arg4), "r"(arg5), "r"(arg6)
        : "rcx", "r11", "memory", "cc");
#elif defined(__aarch64__)
    register uint64_t x0 __asm__("x0") = a1;
    register uint64_t x1 __asm__("x1") = a2;
    register uint64_t x2 __asm__("x2") = a3;
    register uint64_t x3 __asm__("x3") = a4;
    register uint64_t x4 __asm__("x4") = a5;
    register uint64_t x5 __asm__("x5") = a6;
    register uint64_t x8 __asm__("x8") = number;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0), "+r"(x1)
        : "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8)
        : "memory", "cc");
    number = x0;
    status = x1;
#elif defined(__riscv)
    register uint64_t a0 __asm__("a0") = a1;
    register uint64_t a1_reg __asm__("a1") = a2;
    register uint64_t a2_reg __asm__("a2") = a3;
    register uint64_t a3_reg __asm__("a3") = a4;
    register uint64_t a4_reg __asm__("a4") = a5;
    register uint64_t a5_reg __asm__("a5") = a6;
    register uint64_t a7 __asm__("a7") = number;
    __asm__ volatile (
        "ecall"
        : "+r"(a0), "+r"(a1_reg)
        : "r"(a2_reg), "r"(a3_reg), "r"(a4_reg), "r"(a5_reg), "r"(a7)
        : "memory");
    number = a0;
    status = a1_reg;
#else
#error Unsupported libnarc architecture
#endif

    return (narc_result_t) {
        .value = (int64_t)number,
        .status = (uint32_t)status,
        .reserved = 0,
    };
}

static narc_result_t narc_call3(uint64_t id, uint64_t a1, uint64_t a2,
                                uint64_t a3) {
    return narc_call6(id, a1, a2, a3, 0, 0, 0);
}

narc_result_t narc_abi_query(void) {
    return narc_call3(NARC_SYS_ABI_QUERY, 0, 0, 0);
}

narc_result_t narc_getpid(void) {
    return narc_call3(NARC_SYS_GETPID, 0, 0, 0);
}

narc_result_t narc_yield(void) {
    return narc_call3(NARC_SYS_YIELD, 0, 0, 0);
}

_Noreturn void narc_exit(int status) {
    (void)narc_call3(NARC_SYS_EXIT, (uint64_t)(uint32_t)status, 0, 0);
    for (;;) { }
}

narc_result_t narc_open(const char *path, size_t path_length, uint32_t flags) {
    return narc_call3(NARC_SYS_OPEN, (uint64_t)(uintptr_t)path,
                      (uint64_t)path_length, flags);
}

narc_result_t narc_close(int fd) {
    return narc_call3(NARC_SYS_CLOSE, (uint64_t)(uint32_t)fd, 0, 0);
}

narc_result_t narc_read(int fd, void *buffer, size_t length) {
    return narc_call3(NARC_SYS_READ, (uint64_t)(uint32_t)fd,
                      (uint64_t)(uintptr_t)buffer, (uint64_t)length);
}

narc_result_t narc_write(int fd, const void *buffer, size_t length) {
    return narc_call3(NARC_SYS_WRITE, (uint64_t)(uint32_t)fd,
                      (uint64_t)(uintptr_t)buffer, (uint64_t)length);
}

narc_result_t narc_seek(int fd, int64_t offset, uint32_t origin) {
    return narc_call3(NARC_SYS_SEEK, (uint64_t)(uint32_t)fd,
                      (uint64_t)offset, origin);
}
