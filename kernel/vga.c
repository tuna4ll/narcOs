#include <kernel/io.h>
#include <kernel/mm.h>
#include <kernel/vga.h>
#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_PHYS 0x00000000000b8000ULL
#define VGA_VIRT VGA_PHYS
#define VGA_CRTC_INDEX 0x3d4
#define VGA_CRTC_DATA  0x3d5

static volatile uint16_t *vga_buffer;
static size_t vga_row;
static size_t vga_col;
static uint8_t vga_attr = 0x0f; /* white on black */

static uint16_t vga_entry(char c) {
    return (uint16_t)(uint8_t)c | ((uint16_t)vga_attr << 8);
}

static void vga_update_cursor(void) {
    uint16_t pos = (uint16_t)(vga_row * VGA_WIDTH + vga_col);
    outb(VGA_CRTC_INDEX, 0x0f);
    outb(VGA_CRTC_DATA, (uint8_t)(pos & 0xff));
    outb(VGA_CRTC_INDEX, 0x0e);
    outb(VGA_CRTC_DATA, (uint8_t)(pos >> 8));
}

static void vga_scroll(void) {
    if (vga_row < VGA_HEIGHT) return;

    for (size_t row = 1; row < VGA_HEIGHT; row++) {
        for (size_t col = 0; col < VGA_WIDTH; col++) {
            vga_buffer[(row - 1) * VGA_WIDTH + col] =
                vga_buffer[row * VGA_WIDTH + col];
        }
    }

    for (size_t col = 0; col < VGA_WIDTH; col++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = vga_entry(' ');
    }

    vga_row = VGA_HEIGHT - 1;
}

void vga_init(void) {
    vmm_map_kernel(VGA_VIRT, VGA_PHYS, VMM_WRITE);
    vga_buffer = (volatile uint16_t *)(uintptr_t)VGA_VIRT;
    vga_row = 0;
    vga_col = 0;
    vga_clear();
}

void vga_clear(void) {
    if (!vga_buffer) return;

    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = vga_entry(' ');
    }

    vga_row = 0;
    vga_col = 0;
    vga_update_cursor();
}

void vga_putc(char c) {
    if (!vga_buffer) return;

    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\t') {
        size_t spaces = 4 - (vga_col & 3);
        for (size_t i = 0; i < spaces; i++) vga_putc(' ');
        return;
    } else {
        vga_buffer[vga_row * VGA_WIDTH + vga_col] = vga_entry(c);
        vga_col++;
        if (vga_col == VGA_WIDTH) {
            vga_col = 0;
            vga_row++;
        }
    }

    vga_scroll();
    vga_update_cursor();
}

void vga_write(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) vga_putc(s[i]);
}

void vga_puts(const char *s) {
    while (*s) vga_putc(*s++);
}
