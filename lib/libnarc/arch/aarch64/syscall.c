#include "../../src/internal/syscall.h"

narc_result_t __narc_call(uint64_t id, uint64_t a1, uint64_t a2,
                          uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6) {
    register uint64_t x0 __asm__("x0") = a1;
    register uint64_t x1 __asm__("x1") = a2;
    register uint64_t x2 __asm__("x2") = a3;
    register uint64_t x3 __asm__("x3") = a4;
    register uint64_t x4 __asm__("x4") = a5;
    register uint64_t x5 __asm__("x5") = a6;
    register uint64_t x8 __asm__("x8") = NARC_SYSCALL_TAG | id;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0), "+r"(x1)
        : "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8)
        : "memory", "cc");
    return (narc_result_t) { (int64_t)x0, (uint32_t)x1, 0 };
}
