#include <string.h>

size_t strlen(const char *string) {
    size_t length = 0;
    while (string[length]) length++;
    return length;
}
