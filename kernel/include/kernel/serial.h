#pragma once
#include <stdint.h>

void serial_init(void);
void serial_putc(char c);
char serial_getc(void);
void serial_puts(const char *s);
void serial_puthex(uint64_t value);
