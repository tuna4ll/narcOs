#include "../internal/syscall.h"

narc_result_t narc_seek(int fd, int64_t offset, uint32_t origin) {
    return __narc_call3(NARC_SYS_SEEK, (uint64_t)(uint32_t)fd,
                        (uint64_t)offset, origin);
}
