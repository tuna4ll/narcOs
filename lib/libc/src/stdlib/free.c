#include "alloc.h"
#include <stdlib.h>
#include <sys/mman.h>

void free(void *pointer) {
    if (!pointer) return;
    allocation_header_t *header = (allocation_header_t *)pointer - 1;
    munmap(header, header->value.mapped);
}
