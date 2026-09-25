#include "../internal/narc.h"
#include <sched.h>

int sched_yield(void) {
    return (int)__libc_result(narc_yield());
}
