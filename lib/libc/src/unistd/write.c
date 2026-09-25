#include "../internal/narc.h"
#include <unistd.h>

ssize_t write(int fd, const void *buffer, size_t length) {
    return (ssize_t)__libc_result(narc_write(fd, buffer, length));
}
