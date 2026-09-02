#include <libc.h>

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

void puts(const char *s) {
    write(s, strlen(s));
    write("\n", 1);
}
