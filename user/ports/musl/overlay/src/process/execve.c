#include <errno.h>
#include <unistd.h>
#include "syscall.h"

int execve(const char *path, char *const argv[], char *const envp[])
{
	long argc = 0;
	long ret;

	(void)envp;
	if (argv) {
		while (argv[argc]) argc++;
	}
	ret = __syscall(SYS_execve, path, argv, argc);
	if (ret < 0) {
		errno = -ret;
		return -1;
	}
	return ret;
}
