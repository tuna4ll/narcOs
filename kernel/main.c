#include <kernel/gdt.h>
#include <kernel/limine.h>
#include <kernel/serial.h>
#include <stdint.h>

__attribute__((used, section(".limine_requests_start"))) static volatile uint64_t requests_start[] = LIMINE_REQUESTS_START_MARKER;
__attribute__((used, section(".limine_requests"))) static volatile uint64_t base_revision[] = LIMINE_BASE_REVISION(6);
__attribute__((used, section(".limine_requests_end"))) static volatile uint64_t requests_end[] = LIMINE_REQUESTS_END_MARKER;
static uint8_t kernel_stack[16384] __attribute__((aligned(16)));

__attribute__((noreturn)) void _start(void) {
    serial_init();
    serial_puts("[boot] Limine handoff OK\n");
    if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision)) {
        serial_puts("[panic] unsupported Limine base revision\n");
        for (;;) __asm__ volatile ("hlt");
    }
    gdt_init((uint64_t)(uintptr_t)(kernel_stack + sizeof(kernel_stack)));
    serial_puts("[kernel] ring 0 initialized\n");
    for (;;) __asm__ volatile ("hlt");
}
