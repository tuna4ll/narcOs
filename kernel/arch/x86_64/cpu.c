#include <kernel/cpu.h>
#include <kernel/serial.h>
#include <stdint.h>

#define CR0_MP (1ULL << 1)
#define CR0_EM (1ULL << 2)
#define CR0_TS (1ULL << 3)
#define CR0_NE (1ULL << 5)
#define CR4_OSFXSR (1ULL << 9)
#define CR4_OSXMMEXCPT (1ULL << 10)

static void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx,
                  uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile ("cpuid"
                      : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                      : "a"(leaf), "c"(0));
}

void cpu_init(void) {
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    (void)eax;
    (void)ebx;
    (void)ecx;

    /* SSE and SSE2 are part of the x86-64 userspace ABI. */
    if ((edx & (1U << 25)) == 0 || (edx & (1U << 26)) == 0) {
        serial_puts("[panic] CPU lacks SSE/SSE2\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }

    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= CR0_MP | CR0_NE;
    cr0 &= ~(CR0_EM | CR0_TS);
    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0) : "memory");

    uint64_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= CR4_OSFXSR | CR4_OSXMMEXCPT;
    __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4) : "memory");

    /* Give the initial userspace thread a defined x87/SSE state. */
    __asm__ volatile ("fninit");
    const uint32_t mxcsr = 0x1f80;
    __asm__ volatile ("ldmxcsr %0" :: "m"(mxcsr));
}
