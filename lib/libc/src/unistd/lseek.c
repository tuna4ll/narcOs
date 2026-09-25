#include "../internal/narc.h"
#include <errno.h>
#include <unistd.h>

off_t lseek(int fd, off_t offset, int origin) {
    if (origin < SEEK_SET || origin > SEEK_END) {
        errno = EINVAL;
        return -1;
    }
    return (off_t)__libc_result(narc_seek(fd, offset, (uint32_t)origin));
}
