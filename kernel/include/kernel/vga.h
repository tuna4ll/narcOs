#pragma once

#include <stddef.h>
#include <kernel/limine.h>

int vga_init(struct limine_framebuffer_response *response);
void vga_clear(void);
void vga_putc(char c);
void vga_write(const char *s, size_t n);
void vga_puts(const char *s);
