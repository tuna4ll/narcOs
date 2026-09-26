#pragma once

#include <stddef.h>

_Noreturn void abort(void);
_Noreturn void exit(int status);
void *malloc(size_t size);
void free(void *pointer);
void *calloc(size_t count, size_t size);
void *realloc(void *pointer, size_t size);
