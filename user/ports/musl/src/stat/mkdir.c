#include <fcntl.h>
#include <sys/stat.h>
#include "syscall.h"

int mkdir(const char *path, mode_t mode)
{
	return syscall(SYS_mkdirat, AT_FDCWD, path, mode);
}
