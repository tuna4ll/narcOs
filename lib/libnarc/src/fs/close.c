#include "../internal/syscall.h"

narc_result_t narc_close(int fd) {
    return __narc_call3(NARC_SYS_CLOSE, (uint64_t)(uint32_t)fd, 0, 0);
}
