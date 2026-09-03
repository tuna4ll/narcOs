#include <kernel/io.h>
#include <kernel/serial.h>

#define COM1 0x3f8

void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xc7);
    outb(COM1 + 4, 0x0b);
}

void serial_putc(char c) {
    while ((inb(COM1 + 5) & 0x20) == 0) {}
    outb(COM1, (uint8_t)c);
}

void serial_write(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) serial_putc(s[i]);
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
