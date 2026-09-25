#include <string.h>

void *memset(void *dst, int value, size_t length) {
    unsigned char *out = dst;
    for (size_t i = 0; i < length; i++) out[i] = (unsigned char)value;
    return dst;
}
