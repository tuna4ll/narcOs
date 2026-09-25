#include "../internal/syscall.h"

narc_result_t narc_read(int fd, void *buffer, size_t length) {
    return __narc_call3(NARC_SYS_READ, (uint64_t)(uint32_t)fd,
                        (uint64_t)(uintptr_t)buffer, (uint64_t)length);
}
