#include "program_utils.h"

int main(int argc, char** argv) {
    net_ping_result_t result;
    char line[160];
    int status;

    if (argc != 2) {
        userlib_print_error("Usage: ping <host>");
        return 1;
    }

    status = user_net_ping(argv[1], &result);
    if (status == NET_ERR_NOT_READY) {
        userlib_print_error("ping: network driver is not ready");
        return 1;
    }
    if (status == NET_ERR_RESOLVE) {
        userlib_print_error("ping: failed to resolve target host");
        return 1;
    }
    if (status == NET_ERR_IO) {
        userlib_print_error("ping: failed to transmit ICMP packet");
        return 1;
    }

    line[0] = '\0';
    {
        int off = 0;
        if (prog_append_text(line, sizeof(line), &off, "Pinging ") != 0) return 1;
        if (prog_append_text(line, sizeof(line), &off, argv[1]) != 0) return 1;
        if (prog_append_text(line, sizeof(line), &off, " [") != 0) return 1;
        if (prog_append_ip(line, sizeof(line), &off, result.resolved_ip) != 0) return 1;
        if (prog_append_text(line, sizeof(line), &off, "] ...") != 0) return 1;
    }
    if (userlib_println(line) != 0) return 1;

    for (uint32_t i = 0; i < result.attempts; i++) {
        if (result.reply_status[i] == NET_OK) {
            int off = 0;

            line[0] = '\0';
            if (prog_append_text(line, sizeof(line), &off, "Reply from ") != 0) return 1;
            if (prog_append_ip(line, sizeof(line), &off, result.resolved_ip) != 0) return 1;
            if (prog_append_text(line, sizeof(line), &off, ": time=") != 0) return 1;
            if (prog_append_uint(line, sizeof(line), &off, result.rtt_ms[i]) != 0) return 1;
            if (prog_append_text(line, sizeof(line), &off, "ms") != 0) return 1;
            if (userlib_println(line) != 0) return 1;
        } else if (userlib_println("Request timed out.") != 0) {
            return 1;
        }
    }
    return status == NET_OK ? 0 : 1;
}
