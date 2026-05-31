#include <errno.h>
#include "libc.h"

static int narcos_errno;

int *__errno_location(void)
{
	return &narcos_errno;
}

weak_alias(__errno_location, ___errno_location);
