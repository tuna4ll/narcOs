#pragma once

#include <stddef.h>

typedef union allocation_header {
    struct {
        size_t size;
        size_t mapped;
    } value;
    max_align_t alignment;
} allocation_header_t;
