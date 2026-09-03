#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/limine.h>
#include <kernel/mm.h>
#include <kernel/serial.h>
#include <kernel/syscall.h>
#include <kernel/user.h>
#include <kernel/vga.h>
#include <stdint.h>

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t requests_start[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile uint64_t base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
    .response = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
    .response = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = 0,
};

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t requests_end[] = LIMINE_REQUESTS_END_MARKER;

static uint8_t kernel_stack[16384] __attribute__((aligned(16)));

__attribute__((noreturn))
void _start(void) {
    /* Keep COM1 available for early failures, but normal output goes to VGA. */
    serial_init();

    if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision) ||
        !memmap_request.response || !hhdm_request.response || !framebuffer_request.response) {
        serial_puts("[panic] required Limine features unavailable\n");
        for (;;) __asm__ volatile ("hlt");
    }

    mm_init(memmap_request.response, hhdm_request.response->offset);
    if (vga_init(framebuffer_request.response) != 0) {
        for (;;) __asm__ volatile ("cli; hlt");
    }
    vga_puts("[boot] Limine framebuffer ready\n");

    gdt_init((uint64_t)(uintptr_t)(kernel_stack + sizeof(kernel_stack)));
    idt_init();
    syscall_init((uint64_t)(uintptr_t)(kernel_stack + sizeof(kernel_stack)));
    vga_puts("[kernel] ring 0 initialized\n");

    user_start();
    for (;;) __asm__ volatile ("hlt");
}
