#pragma once

#include <stddef.h>

void *memcpy(void *restrict dst, const void *restrict src, size_t length);
void *memmove(void *dst, const void *src, size_t length);
void *memset(void *dst, int value, size_t length);
int memcmp(const void *left, const void *right, size_t length);
size_t strlen(const char *string);
size_t strnlen(const char *string, size_t limit);
int strcmp(const char *left, const char *right);
char *strcpy(char *restrict dst, const char *restrict src);
char *strncpy(char *restrict dst, const char *restrict src, size_t length);
char *strchr(const char *string, int character);
