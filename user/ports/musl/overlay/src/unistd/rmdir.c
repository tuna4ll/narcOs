#include <fcntl.h>
#include <unistd.h>
#include "syscall.h"

int rmdir(const char *path)
{
	return syscall(SYS_unlinkat, AT_FDCWD, path, AT_REMOVEDIR);
}
