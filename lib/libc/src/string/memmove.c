#include <string.h>

void *memmove(void *dst, const void *src, size_t length) {
    unsigned char *out = dst;
    const unsigned char *in = src;
    if (out < in) {
        for (size_t i = 0; i < length; i++) out[i] = in[i];
    } else if (out > in) {
        for (size_t i = length; i; i--) out[i - 1] = in[i - 1];
    }
    return dst;
}
