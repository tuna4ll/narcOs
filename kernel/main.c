#include <kernel/limine.h>
#include <kernel/serial.h>
#include <stdint.h>

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t requests_start[] = LIMINE_REQUESTS_START_MARKER;
__attribute__((used, section(".limine_requests")))
static volatile uint64_t base_revision[] = LIMINE_BASE_REVISION(6);
__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t requests_end[] = LIMINE_REQUESTS_END_MARKER;

__attribute__((noreturn)) void _start(void) {
    serial_init();
    serial_puts("[boot] Limine handoff OK\n");
    if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision))
        serial_puts("[panic] unsupported Limine base revision\n");
    else
        serial_puts("[kernel] ring 0 online\n");
    for (;;) __asm__ volatile ("hlt");
}
