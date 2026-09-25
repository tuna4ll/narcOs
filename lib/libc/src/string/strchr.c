#include <string.h>

char *strchr(const char *string, int character) {
    char value = (char)character;
    for (;; string++) {
        if (*string == value) return (char *)string;
        if (!*string) return 0;
    }
}
