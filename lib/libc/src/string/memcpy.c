#include <string.h>

void *memcpy(void *restrict dst, const void *restrict src, size_t length) {
    unsigned char *out = dst;
    const unsigned char *in = src;
    for (size_t i = 0; i < length; i++) out[i] = in[i];
    return dst;
}
