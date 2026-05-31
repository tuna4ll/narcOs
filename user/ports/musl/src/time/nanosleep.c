#include <errno.h>
#include <time.h>
#include "syscall.h"

int nanosleep(const struct timespec *req, struct timespec *rem)
{
	long ret;

	(void)rem;
	ret = __syscall(SYS_nanosleep, req, 0);
	if (ret < 0) {
		errno = -ret;
		return -1;
	}
	return 0;
}
