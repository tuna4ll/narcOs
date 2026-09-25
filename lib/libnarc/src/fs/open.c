#include "../internal/syscall.h"

narc_result_t narc_open(const char *path, size_t path_length, uint32_t flags) {
    return __narc_call3(NARC_SYS_OPEN, (uint64_t)(uintptr_t)path,
                        (uint64_t)path_length, flags);
}
