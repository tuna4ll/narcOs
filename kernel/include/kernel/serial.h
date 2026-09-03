#pragma once
#include <stddef.h>
#include <stdint.h>
void serial_init(void);
void serial_putc(char c);
void serial_write(const char *s, size_t n);
void serial_puts(const char *s);
void serial_puthex(uint64_t value);
