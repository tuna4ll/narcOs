#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "syscall.h"

int access(const char *filename, int amode)
{
	long ret = __syscall(SYS_faccessat, AT_FDCWD, filename, amode, 0);
	if (ret < 0) {
		errno = -ret;
		return -1;
	}
	return 0;
}
