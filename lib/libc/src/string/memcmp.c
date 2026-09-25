#include <string.h>

int memcmp(const void *left, const void *right, size_t length) {
    const unsigned char *a = left;
    const unsigned char *b = right;
    for (size_t i = 0; i < length; i++)
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}
