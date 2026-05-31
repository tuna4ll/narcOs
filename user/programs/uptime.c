#include <stdint.h>
#include <stdio.h>
#include <time.h>

static int put_uint(uint32_t value) {
    char digits[16];
    int count = 0;

    if (value == 0U) return putchar('0') == EOF ? -1 : 0;
    while (value != 0U && count < (int)sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (count > 0) {
        if (putchar(digits[--count]) == EOF) return -1;
    }
    return 0;
}

int main(void) {
    struct timespec now;

    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        fputs("uptime: clock_gettime failed\n", stderr);
        return 1;
    }
    if (fputs("System Uptime (seconds): ", stdout) < 0) return 1;
    if (put_uint((uint32_t)now.tv_sec) != 0) return 1;
    return putchar('\n') == EOF ? 1 : 0;
}
