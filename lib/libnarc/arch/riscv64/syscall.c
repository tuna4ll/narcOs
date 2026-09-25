#include "../../src/internal/syscall.h"

narc_result_t __narc_call(uint64_t id, uint64_t a1, uint64_t a2,
                          uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6) {
    register uint64_t a0 __asm__("a0") = a1;
    register uint64_t a1_reg __asm__("a1") = a2;
    register uint64_t a2_reg __asm__("a2") = a3;
    register uint64_t a3_reg __asm__("a3") = a4;
    register uint64_t a4_reg __asm__("a4") = a5;
    register uint64_t a5_reg __asm__("a5") = a6;
    register uint64_t a7 __asm__("a7") = NARC_SYSCALL_TAG | id;
    __asm__ volatile (
        "ecall"
        : "+r"(a0), "+r"(a1_reg)
        : "r"(a2_reg), "r"(a3_reg), "r"(a4_reg), "r"(a5_reg), "r"(a7)
        : "memory");
    return (narc_result_t) { (int64_t)a0, (uint32_t)a1_reg, 0 };
}
