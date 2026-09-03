#include <kernel/serial.h>
#include <kernel/vga.h>
#include <stdint.h>

static uint64_t read_cr2(void) {
    uint64_t v;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(v));
    return v;
}

__attribute__((noreturn))
void exception_die(uint64_t vector, uint64_t error, uint64_t rip, uint64_t cs) {
    serial_puts("[fault] vector=");
    serial_puthex(vector);
    serial_puts(" error=");
    serial_puthex(error);
    serial_puts(" rip=");
    serial_puthex(rip);
    serial_puts(" cs=");
    serial_puthex(cs);
    if (vector == 14) {
        serial_puts(" cr2=");
        serial_puthex(read_cr2());
    }
    serial_puts("\n");

    vga_puts("[fault] userspace exception; see serial\n");
    for (;;) __asm__ volatile ("cli; hlt");
}
