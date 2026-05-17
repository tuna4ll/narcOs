#include "net_http_common.h"

int main(int argc, char** argv) {
    char args[192];
    char host[96];
    char path[160];
    char response[4096];
    net_http_result_t result;
    int status;

    if (argc < 2 || prog_join_args(argc, argv, 1, args, sizeof(args)) != 0) {
        userlib_print_error("Usage: http <host> [path]");
        return 1;
    }
    if (prog_parse_http_target(args, host, sizeof(host), path, sizeof(path)) != 0) {
        userlib_print_error("Usage: http <host> [path]");
        return 1;
    }

    status = prog_http_fetch_text(host, path, response, sizeof(response), &result);
    if (status != NET_OK) {
        char line[160];
        int off = 0;

        line[0] = '\0';
        if (prog_append_text(line, sizeof(line), &off, "http: request failed: ") != 0) return 1;
        if (prog_append_text(line, sizeof(line), &off, prog_net_error_string(status)) != 0) return 1;
        userlib_print_error(line);
        return 1;
    }
    return prog_print_http_response(args, response, &result) == 0 ? 0 : 1;
}
