#pragma once

#include <stddef.h>
#include <stdint.h>
#include <kernel/limine.h>

int console_init(struct limine_framebuffer_response *response);
void console_write(const char *s, size_t n);
void console_puts(const char *s);
uint16_t console_cols(void);
uint16_t console_rows(void);
