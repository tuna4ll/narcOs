#include "../internal/narc.h"
#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>

void *mmap(void *address, size_t length, int protection, int flags, int fd,
           off_t offset) {
    const int known_protection = PROT_READ | PROT_WRITE | PROT_EXEC;
    const int known_flags = MAP_SHARED | MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS;
    if (address || !length || (protection & ~known_protection) ||
        !(protection & PROT_READ) || (protection & PROT_EXEC) ||
        (flags & ~known_flags) || !(flags & MAP_ANONYMOUS) ||
        !!(flags & MAP_SHARED) == !!(flags & MAP_PRIVATE) ||
        (flags & MAP_FIXED) || fd != -1 || offset != 0) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    uint32_t native_flags = NARC_MAP_READ;
    if (protection & PROT_WRITE) native_flags |= NARC_MAP_WRITE;
    long result = __libc_result(narc_map(length, native_flags));
    return result < 0 ? MAP_FAILED : (void *)(uintptr_t)result;
}
