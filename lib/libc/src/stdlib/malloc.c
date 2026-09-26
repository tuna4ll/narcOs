#include "alloc.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

#define PAGE_SIZE 4096u

void *malloc(size_t size) {
    if (!size) size = 1;
    if (size > SIZE_MAX - sizeof(allocation_header_t) - (PAGE_SIZE - 1)) {
        errno = ENOMEM;
        return 0;
    }

    size_t mapped = (size + sizeof(allocation_header_t) + PAGE_SIZE - 1) &
                    ~(size_t)(PAGE_SIZE - 1);
    allocation_header_t *header = mmap(0, mapped, PROT_READ | PROT_WRITE,
                                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (header == MAP_FAILED) return 0;
    header->value.size = size;
    header->value.mapped = mapped;
    return header + 1;
}
