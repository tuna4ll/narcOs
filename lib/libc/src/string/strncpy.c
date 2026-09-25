#include <string.h>

char *strncpy(char *restrict dst, const char *restrict src, size_t length) {
    size_t i = 0;
    for (; i < length && src[i]; i++) dst[i] = src[i];
    for (; i < length; i++) dst[i] = 0;
    return dst;
}
