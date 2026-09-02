#pragma once
#include <stddef.h>
#include <stdint.h>

long write(const void *buf, size_t len);
__attribute__((noreturn)) void exit(int status);
size_t strlen(const char *s);
void puts(const char *s);
