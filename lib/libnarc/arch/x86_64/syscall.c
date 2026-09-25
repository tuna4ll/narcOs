#include "../../src/internal/syscall.h"

narc_result_t __narc_call(uint64_t id, uint64_t a1, uint64_t a2,
                          uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6) {
    uint64_t number = NARC_SYSCALL_TAG | id;
    uint64_t status = a3;
    register uint64_t arg4 __asm__("r10") = a4;
    register uint64_t arg5 __asm__("r8") = a5;
    register uint64_t arg6 __asm__("r9") = a6;
    __asm__ volatile (
        "syscall"
        : "+a"(number), "+d"(status)
        : "D"(a1), "S"(a2), "r"(arg4), "r"(arg5), "r"(arg6)
        : "rcx", "r11", "memory", "cc");
    return (narc_result_t) { (int64_t)number, (uint32_t)status, 0 };
}
