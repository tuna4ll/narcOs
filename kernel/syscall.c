#include <kernel/io.h>
#include <kernel/serial.h>
#include <kernel/syscall.h>
#include <stddef.h>
#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT  60

static int user_range_ok(uint64_t ptr, uint64_t len) {
    const uint64_t user_lo = 0x0000100000000000ULL;
    const uint64_t user_hi = 0x0000100000400000ULL;
    if (ptr < user_lo || ptr >= user_hi) return 0;
    if (len > user_hi - ptr) return 0;
    return 1;
}

void syscall_dispatch(struct syscall_frame *frame) {
    if (frame->rax == SYS_WRITE) {
        const char *buf = (const char *)(uintptr_t)frame->rdi;
        size_t len = (size_t)frame->rsi;
        if (user_range_ok(frame->rdi, frame->rsi)) serial_write(buf, len);
        return;
    }

    if (frame->rax == SYS_EXIT) {
        serial_puts("[kernel] userspace exited\n");
        outb(0xf4, (uint8_t)(frame->rdi << 1 | 1));
        for (;;) __asm__ volatile ("hlt");
    }

    serial_puts("[kernel] unknown syscall\n");
}
