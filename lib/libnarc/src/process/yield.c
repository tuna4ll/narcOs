#include "../internal/syscall.h"

narc_result_t narc_yield(void) {
    return __narc_call3(NARC_SYS_YIELD, 0, 0, 0);
}
