#include <fcntl.h>
#include <stdio.h>
#include "syscall.h"

int remove(const char *path)
{
	long ret = __syscall(SYS_unlinkat, AT_FDCWD, path, 0);
	if (ret < 0) ret = __syscall(SYS_unlinkat, AT_FDCWD, path, AT_REMOVEDIR);
	return __syscall_ret(ret);
}
