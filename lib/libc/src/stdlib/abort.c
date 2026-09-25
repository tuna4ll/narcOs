#include <stdlib.h>
#include <unistd.h>

_Noreturn void abort(void) {
    _exit(134);
}
