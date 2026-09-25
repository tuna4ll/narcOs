#include <errno.h>

static int value;

int *__errno_location(void) {
    return &value;
}
