#define _BSD_SOURCE
#include <errno.h>
#include <unistd.h>
#include "syscall.h"

int brk(void *end)
{
	void *ret = (void *)__syscall(SYS_brk, end);

	if (ret != end) {
		errno = ENOMEM;
		return -1;
	}
	return 0;
}
