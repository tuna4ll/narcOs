#include "../internal/narc.h"
#include <unistd.h>

ssize_t read(int fd, void *buffer, size_t length) {
    return (ssize_t)__libc_result(narc_read(fd, buffer, length));
}
