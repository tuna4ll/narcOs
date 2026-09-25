#include <string.h>

char *strcpy(char *restrict dst, const char *restrict src) {
    char *result = dst;
    while ((*dst++ = *src++)) { }
    return result;
}
