#include "../internal/syscall.h"

_Noreturn void narc_exit(int status) {
    (void)__narc_call3(NARC_SYS_EXIT, (uint64_t)(uint32_t)status, 0, 0);
    for (;;) { }
}
