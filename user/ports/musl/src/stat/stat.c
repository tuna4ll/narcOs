#include <errno.h>
#include <sys/stat.h>
#include "syscall.h"
#include "narcos_stat.h"

int stat(const char *restrict path, struct stat *restrict st)
{
	struct narcos_kstat kst;
	long ret;

	ret = __syscall(SYS_stat, path, &kst);
	if (ret < 0) {
		errno = -ret;
		return -1;
	}
	narcos_copy_stat(st, &kst);
	return 0;
}
