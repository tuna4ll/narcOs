#define _BSD_SOURCE
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include "syscall.h"

void *sbrk(intptr_t inc)
{
	uintptr_t cur = (uintptr_t)__syscall(SYS_brk, 0);
	uintptr_t next;
	uintptr_t ret;

	if (!cur) {
		errno = ENOMEM;
		return (void *)-1;
	}
	if (inc == 0) return (void *)cur;
	next = cur + (uintptr_t)inc;
	if ((inc > 0 && next < cur) || (inc < 0 && next > cur)) {
		errno = ENOMEM;
		return (void *)-1;
	}
	ret = (uintptr_t)__syscall(SYS_brk, next);
	if (ret != next) {
		errno = ENOMEM;
		return (void *)-1;
	}
	return (void *)cur;
}
