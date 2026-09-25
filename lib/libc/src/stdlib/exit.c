#include <stdlib.h>
#include <unistd.h>

_Noreturn void exit(int status) {
    _exit(status);
}
