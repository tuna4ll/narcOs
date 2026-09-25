#include "../internal/syscall.h"

narc_result_t narc_abi_query(void) {
    return __narc_call3(NARC_SYS_ABI_QUERY, 0, 0, 0);
}
