#include "program_utils.h"

int main(void) {
    net_ipv4_config_t config;

    userlib_memset(&config, 0, sizeof(config));
    if (user_net_get_config(&config) != 0 || !config.available) {
        userlib_print_error("Network: driver not ready.");
        return 1;
    }

    if (userlib_println("Network Driver : RTL8139") != 0) return 1;
    if (userlib_println(config.configured ? "Address Mode   : DHCP" : "Address Mode   : Unconfigured") != 0) return 1;
    if (config.configured) {
        if (prog_print_ip_line("IP             : ", config.ip_addr) != 0) return 1;
        if (prog_print_ip_line("Netmask        : ", config.netmask) != 0) return 1;
        if (prog_print_ip_line("Gateway        : ", config.gateway) != 0) return 1;
        if (prog_print_ip_line("DNS            : ", config.dns_server) != 0) return 1;
    }
    return 0;
}
