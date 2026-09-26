#include "../internal/narc.h"
#include <sys/mman.h>

int munmap(void *address, size_t length) {
    return (int)__libc_result(narc_unmap(address, length));
}
