#include "program_utils.h"

int main(int argc, char** argv) {
    uint32_t ip_addr = 0;
    char line[160];
    int off = 0;

    if (argc != 2) {
        userlib_print_error("Usage: dns <host>");
        return 1;
    }
    if (user_net_resolve(argv[1], &ip_addr) != NET_OK) {
        userlib_print_error("dns: lookup failed");
        return 1;
    }

    line[0] = '\0';
    if (prog_append_text(line, sizeof(line), &off, argv[1]) != 0) return 1;
    if (prog_append_text(line, sizeof(line), &off, " -> ") != 0) return 1;
    if (prog_append_ip(line, sizeof(line), &off, ip_addr) != 0) return 1;
    return userlib_println(line) == 0 ? 0 : 1;
}
