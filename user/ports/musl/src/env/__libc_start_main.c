#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "libc.h"
#if defined(__x86_64__)
#include "pthread_impl.h"
#endif

static void dummy(void) {}
weak_alias(dummy, _init);

extern weak hidden void (*const __init_array_start)(void), (*const __init_array_end)(void);

static void libc_start_init(void)
{
	uintptr_t a;

	_init();
	for (a = (uintptr_t)&__init_array_start; a < (uintptr_t)&__init_array_end; a += sizeof(void (*)())) {
		(*(void (**)(void))a)();
	}
}

#if defined(__x86_64__)
static struct {
	char c;
	struct pthread pt;
	void *space[16];
} narcos_initial_tls;
#endif

static size_t narcos_empty_auxv[] = { 0, 0 };

int __libc_start_main(int (*main)(int, char **, char **), int argc, char **argv,
	void (*init_dummy)(), void (*fini_dummy)(), void (*ldso_dummy)())
{
	char **envp;

	(void)init_dummy;
	(void)fini_dummy;
	(void)ldso_dummy;
#if defined(__x86_64__)
	if (__init_tp(&narcos_initial_tls.pt) < 0) _Exit(127);
#endif
	envp = argv + argc + 1;
	__environ = envp;
	libc.auxv = narcos_empty_auxv;
	libc.page_size = 4096;
	libc_start_init();
	exit(main(argc, argv, envp));
	return 0;
}
