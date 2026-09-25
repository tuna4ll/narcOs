#include "../internal/syscall.h"

narc_result_t narc_write(int fd, const void *buffer, size_t length) {
    return __narc_call3(NARC_SYS_WRITE, (uint64_t)(uint32_t)fd,
                        (uint64_t)(uintptr_t)buffer, (uint64_t)length);
}
