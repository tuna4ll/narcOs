#include "../internal/narc.h"
#include <unistd.h>

int close(int fd) {
    return (int)__libc_result(narc_close(fd));
}
