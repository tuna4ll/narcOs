#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "syscall.h"
#include "narcos_stat.h"

int fstatat(int fd, const char *restrict path, struct stat *restrict st, int flag)
{
	struct narcos_kstat kst;
	long ret;

	(void)flag;
	ret = __syscall(SYS_fstatat, fd, path, &kst, flag);
	if (ret < 0) {
		errno = -ret;
		return -1;
	}
	narcos_copy_stat(st, &kst);
	return 0;
}
