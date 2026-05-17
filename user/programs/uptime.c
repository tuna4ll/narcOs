#include "program_utils.h"

int main(void) {
    char line[96];
    int off = 0;

    line[0] = '\0';
    if (prog_append_text(line, sizeof(line), &off, "System Uptime (seconds): ") != 0) return 1;
    if (prog_append_uint(line, sizeof(line), &off, user_uptime_ticks() / 100U) != 0) return 1;
    return userlib_println(line) == 0 ? 0 : 1;
}
