#include "../internal/syscall.h"

narc_result_t narc_unmap(void *address, size_t length) {
    return __narc_call3(NARC_SYS_UNMAP, (uint64_t)(uintptr_t)address,
                        (uint64_t)length, 0);
}
