#include "alloc.h"
#include <stdlib.h>
#include <string.h>

void *realloc(void *pointer, size_t size) {
    if (!pointer) return malloc(size);
    if (!size) {
        free(pointer);
        return 0;
    }

    allocation_header_t *header = (allocation_header_t *)pointer - 1;
    if (size <= header->value.size) {
        header->value.size = size;
        return pointer;
    }

    void *replacement = malloc(size);
    if (!replacement) return 0;
    memcpy(replacement, pointer, header->value.size);
    free(pointer);
    return replacement;
}
