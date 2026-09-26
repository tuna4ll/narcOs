#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void *calloc(size_t count, size_t size) {
    if (size && count > SIZE_MAX / size) {
        errno = ENOMEM;
        return 0;
    }
    size_t total = count * size;
    void *pointer = malloc(total);
    if (pointer) memset(pointer, 0, total);
    return pointer;
}
