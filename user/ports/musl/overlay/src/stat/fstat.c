#include <errno.h>
#include <sys/stat.h>
#include "syscall.h"
#include "narcos_stat.h"

int fstat(int fd, struct stat *st)
{
	struct narcos_kstat kst;
	long ret;

	ret = __syscall(SYS_fstat, fd, &kst);
	if (ret < 0) {
		errno = -ret;
		return -1;
	}
	narcos_copy_stat(st, &kst);
	return 0;
}
