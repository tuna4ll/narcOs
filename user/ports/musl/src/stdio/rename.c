#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include "syscall.h"

int rename(const char *old, const char *new)
{
	long ret = __syscall(SYS_renameat, AT_FDCWD, old, AT_FDCWD, new);
	if (ret < 0) {
		errno = -ret;
		return -1;
	}
	return 0;
}
