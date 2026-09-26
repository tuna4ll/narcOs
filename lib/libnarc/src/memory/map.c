#include "../internal/syscall.h"

narc_result_t narc_map(size_t length, uint32_t flags) {
    return __narc_call3(NARC_SYS_MAP, (uint64_t)length, flags, 0);
}
