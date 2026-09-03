#include <kernel/syscall.h>
#include <stdint.h>

#define MSR_EFER  0xc0000080u
#define MSR_STAR  0xc0000081u
#define MSR_LSTAR 0xc0000082u
#define MSR_FMASK 0xc0000084u
#define EFER_SCE  (1ULL << 0)

uint64_t syscall_kernel_rsp;
uint64_t syscall_user_rsp;

extern void syscall_fast_entry(void);

static uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t lo = (uint32_t)value;
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

void syscall_init(uint64_t kernel_rsp) {
    syscall_kernel_rsp = kernel_rsp & ~0xfULL;

    /* SYSRET derives SS=base+8 and CS=base+16. With our GDT that means
       base 0x13 -> SS 0x1b and CS 0x23. */
    uint64_t star = (0x13ULL << 48) | (0x08ULL << 32);
    wrmsr(MSR_STAR, star);
    wrmsr(MSR_LSTAR, (uint64_t)(uintptr_t)syscall_fast_entry);
    wrmsr(MSR_FMASK, 0x600ULL); /* clear IF and DF while handling a syscall */
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | EFER_SCE);
}
