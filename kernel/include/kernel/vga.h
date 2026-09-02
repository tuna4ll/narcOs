#pragma once

#include <stddef.h>

void vga_init(void);
void vga_clear(void);
void vga_putc(char c);
void vga_write(const char *s, size_t n);
void vga_puts(const char *s);
