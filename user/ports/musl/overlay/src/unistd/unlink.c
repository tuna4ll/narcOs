#include <fcntl.h>
#include <unistd.h>
#include "syscall.h"

int unlink(const char *path)
{
	return syscall(SYS_unlinkat, AT_FDCWD, path, 0);
}
