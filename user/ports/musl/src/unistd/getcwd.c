#include <errno.h>
#include <unistd.h>
#include "syscall.h"

char *getcwd(char *buf, size_t size)
{
	long ret;

	if (!buf || !size) {
		errno = EINVAL;
		return 0;
	}
	ret = __syscall(SYS_getcwd, buf, size);
	if (ret < 0) {
		errno = -ret;
		return 0;
	}
	return buf;
}
