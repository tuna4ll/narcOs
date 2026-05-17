#include "program_utils.h"

int main(void) {
    rtc_local_time_t now;
    char datetime[64];
    char line[96];
    int off = 0;

    if (user_get_local_time(&now) != 0) {
        userlib_print_error("time: failed to read RTC");
        return 1;
    }
    if (prog_format_datetime(datetime, sizeof(datetime), &now, 0) != 0) return 1;
    line[0] = '\0';
    if (prog_append_text(line, sizeof(line), &off, "Current Local Time: ") != 0) return 1;
    if (prog_append_text(line, sizeof(line), &off, datetime + 11) != 0) return 1;
    return userlib_println(line) == 0 ? 0 : 1;
}
