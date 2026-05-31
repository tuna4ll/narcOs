#include <errno.h>
#include <sys/wait.h>
#include <narcos/syscall.h>
#include "syscall.h"

pid_t waitpid(pid_t pid, int *status, int options)
{
	long ret = __syscall(SYS_waitpid, pid, status, options);
	if (ret < 0) {
		errno = ECHILD;
		return -1;
	}
	return (pid_t)ret;
}
