#include <libc.h>

void _start(void) {
    puts("Hello from userspace");
    exit(0);
}
