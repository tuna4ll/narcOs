#include "net_http_common.h"

int main(int argc, char** argv) {
    char args[192];
    char host[96];
    char path[160];
    char response[2048];
    net_http_result_t result;
    int status;

    if (argc < 2 || prog_join_args(argc, argv, 1, args, sizeof(args)) != 0) {
        userlib_print_error("Usage: netdemo <host> [path]");
        return 1;
    }
    if (prog_parse_http_target(args, host, sizeof(host), path, sizeof(path)) != 0) {
        userlib_print_error("Usage: netdemo <host> [path]");
        return 1;
    }

    if (userlib_println("Ring3 netdemo: HTTP request starting") != 0) return 1;
    status = prog_http_fetch_text(host, path, response, sizeof(response), &result);
    if (status != NET_OK) {
        userlib_print_error("Ring3 netdemo: HTTP request failed");
        return 1;
    }
    if (userlib_println("Ring3 netdemo: response") != 0) return 1;
    if (userlib_println(response[0] != '\0' ? response : "(empty response)") != 0) return 1;
    if (result.truncated != 0U && userlib_println("Ring3 netdemo: response truncated") != 0) return 1;
    if (result.complete == 0U &&
        userlib_println("Ring3 netdemo: connection timed out before clean close") != 0) {
        return 1;
    }
    return 0;
}
