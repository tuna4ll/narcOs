#include "user_abi.h"
#include <stdio.h>

static int put_two_digits(unsigned value) {
    if (value < 10U && putchar('0') == EOF) return -1;
    if (putchar((int)('0' + (value / 10U) % 10U)) == EOF) return -1;
    return putchar((int)('0' + value % 10U)) == EOF ? -1 : 0;
}

int main(void) {
    rtc_local_time_t now;

    if (user_get_local_time(&now) != 0) {
        fputs("date: failed to read RTC\n", stderr);
        return 1;
    }
    if (fputs("Current Local Date: 20", stdout) < 0) return 1;
    if (put_two_digits(now.year) != 0) return 1;
    if (putchar('-') == EOF) return 1;
    if (put_two_digits(now.month) != 0) return 1;
    if (putchar('-') == EOF) return 1;
    if (put_two_digits(now.day) != 0) return 1;
    return putchar('\n') == EOF ? 1 : 0;
}
