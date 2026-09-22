#include <kernel/mm.h>
#include <kernel/serial.h>

#if defined(__x86_64__)
#include <kernel/io.h>
#define COM1 0x3f8
#elif defined(__aarch64__)
#define UART_BASE 0x09000000ULL
static volatile uint32_t *uart;
#endif

void serial_init(void) {
#if defined(__x86_64__)
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xc7);
    outb(COM1 + 4, 0x0b);
#elif defined(__aarch64__)
    vmm_map_kernel((uint64_t)(uintptr_t)phys_to_virt(UART_BASE), UART_BASE,
                   VMM_WRITE | VMM_DEVICE);
    uart = phys_to_virt(UART_BASE);
#endif
}

void serial_putc(char c) {
#if defined(__x86_64__)
    while ((inb(COM1 + 5) & 0x20) == 0) {}
    outb(COM1, (uint8_t)c);
#elif defined(__aarch64__)
    while (uart[6] & (1U << 5)) {}
    uart[0] = (uint32_t)c;
#else
    register uint64_t a0 __asm__("a0") = (uint8_t)c;
    register uint64_t a7 __asm__("a7") = 1;
    __asm__ volatile ("ecall" : "+r"(a0) : "r"(a7) : "memory");
#endif
}

char serial_getc(void) {
#if defined(__x86_64__)
    while (!(inb(COM1 + 5) & 1)) {}
    return (char)inb(COM1);
#elif defined(__aarch64__)
    while (uart[6] & (1U << 4)) {}
    return (char)uart[0];
#else
    for (;;) {
        register long a0 __asm__("a0");
        register uint64_t a7 __asm__("a7") = 2;
        __asm__ volatile ("ecall" : "=r"(a0) : "r"(a7) : "memory");
        if (a0 >= 0) return (char)a0;
    }
#endif
}

void serial_puts(const char *s) {
    while (*s) serial_putc(*s++);
}

void serial_puthex(uint64_t value) {
    static const char hex[] = "0123456789abcdef";
    serial_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        serial_putc(hex[(value >> shift) & 0xf]);
}
