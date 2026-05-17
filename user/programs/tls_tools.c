#define PROGRAM_ENABLE_TLS
#include "net_http_common.h"

static const char* tls_tools_basename(const char* path) {
    const char* base = path;

    if (!path) return "";
    while (*path) {
        if (*path == '/') base = path + 1;
        path++;
    }
    return base;
}

static int tls_tools_https_main(int argc, char** argv) {
    char args[192];
    char host[96];
    char path[160];
    char response[4096];
    char line[192];
    net_http_result_t result;
    int status;

    if (argc < 2 || prog_join_args(argc, argv, 1, args, sizeof(args)) != 0) {
        userlib_print_error("Usage: https https://<pinned-host>/<path>");
        return 1;
    }
    if (prog_parse_http_target(args, host, sizeof(host), path, sizeof(path)) != 0) {
        userlib_print_error("Usage: https https://<pinned-host>/<path>");
        return 1;
    }

    status = prog_https_fetch_text(host, path, response, sizeof(response), &result);
    if (status != NET_OK) {
        int off = 0;

        line[0] = '\0';
        if (prog_append_text(line, sizeof(line), &off, "https: request failed: ") != 0) return 1;
        if (prog_append_text(line, sizeof(line), &off, user_tls_error_string(status)) != 0) return 1;
        userlib_print_error(line);

        line[0] = '\0';
        off = 0;
        if (prog_append_text(line, sizeof(line), &off, "https stage: ") != 0) return 1;
        if (prog_append_text(line, sizeof(line), &off, user_tls_debug_stage_name()) != 0) return 1;
        if (userlib_println(line) != 0) return 1;
        if (user_tls_debug_detail()[0] != '\0') {
            line[0] = '\0';
            off = 0;
            if (prog_append_text(line, sizeof(line), &off, "https detail: ") != 0) return 1;
            if (prog_append_text(line, sizeof(line), &off, user_tls_debug_detail()) != 0) return 1;
            if (userlib_println(line) != 0) return 1;
        }
        return 1;
    }
    return prog_print_http_response(args, response, &result) == 0 ? 0 : 1;
}

static int tls_tools_fetch_main(int argc, char** argv) {
    char args[256];
    char host[96];
    char path[160];
    char output_path[128];
    char response[4096];
    char line[192];
    net_http_result_t result;
    int status;
    int use_https = 0;
    uint32_t body_offset;
    uint32_t body_len;
    const void* body_ptr;
    int off = 0;

    if (argc < 3 || prog_join_args(argc, argv, 1, args, sizeof(args)) != 0) {
        userlib_print_error("Usage: fetch <host> [path] <output-file>");
        userlib_print_error("   or: fetch https://<pinned-host>/<path> <output-file>");
        return 1;
    }
    if (prog_parse_fetch_args(args, host, sizeof(host), path, sizeof(path),
                              output_path, sizeof(output_path), &use_https) != 0) {
        userlib_print_error("Usage: fetch <host> [path] <output-file>");
        userlib_print_error("   or: fetch https://<pinned-host>/<path> <output-file>");
        return 1;
    }

    if (userlib_println("Ring3 fetch: downloading") != 0) return 1;
    if (use_https) {
        status = prog_https_fetch_text(host, path, response, sizeof(response), &result);
    } else {
        status = prog_http_fetch_text(host, path, response, sizeof(response), &result);
    }
    if (status != NET_OK) {
        line[0] = '\0';
        if (prog_append_text(line, sizeof(line), &off, "fetch: ") != 0) return 1;
        if (prog_append_text(line, sizeof(line), &off,
                             use_https ? user_tls_error_string(status) : prog_net_error_string(status)) != 0) {
            return 1;
        }
        userlib_print_error(line);
        if (use_https) {
            line[0] = '\0';
            off = 0;
            if (prog_append_text(line, sizeof(line), &off, "fetch https stage: ") != 0) return 1;
            if (prog_append_text(line, sizeof(line), &off, user_tls_debug_stage_name()) != 0) return 1;
            if (userlib_println(line) != 0) return 1;
            if (user_tls_debug_detail()[0] != '\0') {
                line[0] = '\0';
                off = 0;
                if (prog_append_text(line, sizeof(line), &off, "fetch https detail: ") != 0) return 1;
                if (prog_append_text(line, sizeof(line), &off, user_tls_debug_detail()) != 0) return 1;
                if (userlib_println(line) != 0) return 1;
            }
        }
        return 1;
    }

    body_offset = prog_http_find_body(response, result.response_len);
    body_ptr = response;
    body_len = result.response_len;
    if (body_offset < body_len) {
        body_ptr = response + body_offset;
        body_len -= body_offset;
    }
    if (user_fs_write_raw(output_path, body_ptr, body_len) != (int)body_len) {
        userlib_print_error("Ring3 fetch: failed to write file");
        return 1;
    }

    line[0] = '\0';
    off = 0;
    if (prog_append_text(line, sizeof(line), &off, "Saved ") != 0) return 1;
    if (prog_append_uint(line, sizeof(line), &off, body_len) != 0) return 1;
    if (prog_append_text(line, sizeof(line), &off, " bytes to ") != 0) return 1;
    if (prog_append_text(line, sizeof(line), &off, output_path) != 0) return 1;
    if (userlib_println(line) != 0) return 1;
    if (result.truncated != 0U && userlib_println("warning: Response truncated to local buffer size.") != 0) return 1;
    if (result.complete == 0U &&
        userlib_println("warning: Remote peer did not close cleanly before timeout.") != 0) {
        return 1;
    }
    return 0;
}

static int print_summary_line(const char* label, uint32_t value) {
    char line[96];
    int off = 0;

    line[0] = '\0';
    if (prog_append_text(line, sizeof(line), &off, label) != 0) return -1;
    if (prog_append_uint(line, sizeof(line), &off, value) != 0) return -1;
    return userlib_println(line);
}

static int print_case(const user_tls_selftest_case_t* test_case) {
    char line[192];
    int off = 0;

    if (!test_case) return 0;
    line[0] = '\0';
    if (prog_append_text(line, sizeof(line), &off, "[") != 0) return -1;
    if (prog_append_text(line, sizeof(line), &off, user_tls_test_status_name(test_case->status)) != 0) return -1;
    if (prog_append_text(line, sizeof(line), &off, "] ") != 0) return -1;
    if (prog_append_text(line, sizeof(line), &off, test_case->name) != 0) return -1;
    if (test_case->detail[0] != '\0') {
        if (prog_append_text(line, sizeof(line), &off, " - ") != 0) return -1;
        if (prog_append_text(line, sizeof(line), &off, test_case->detail) != 0) return -1;
    }
    return userlib_println(line);
}

static int tls_tools_test_main(int argc, char** argv) {
    user_tls_selftest_report_t report;
    int status;

    (void)argv;
    if (argc != 1) {
        userlib_print_error("Usage: tls_test");
        return 1;
    }

    status = user_tls_run_selftests(&report);
    if (userlib_println("TLS Self-Test") != 0) return 1;
    if (print_summary_line("Total          : ", report.total_count) != 0) return 1;
    if (print_summary_line("Passed         : ", report.pass_count) != 0) return 1;
    if (print_summary_line("Failed         : ", report.fail_count) != 0) return 1;
    if (print_summary_line("Pending        : ", report.pending_count) != 0) return 1;
    if (print_summary_line("Skipped        : ", report.skip_count) != 0) return 1;
    for (uint32_t i = 0; i < report.total_count && i < USER_TLS_SELFTEST_MAX_CASES; i++) {
        if (print_case(&report.cases[i]) != 0) return 1;
    }
    return status == 0 ? 0 : 1;
}

int main(int argc, char** argv) {
    const char* command = argc > 0 ? tls_tools_basename(argv[0]) : "";

    if (userlib_strcmp(command, "https") == 0) return tls_tools_https_main(argc, argv);
    if (userlib_strcmp(command, "fetch") == 0) return tls_tools_fetch_main(argc, argv);
    if (userlib_strcmp(command, "tls_test") == 0) return tls_tools_test_main(argc, argv);

    if (argc >= 2) {
        command = argv[1];
        if (userlib_strcmp(command, "https") == 0) return tls_tools_https_main(argc - 1, argv + 1);
        if (userlib_strcmp(command, "fetch") == 0) return tls_tools_fetch_main(argc - 1, argv + 1);
        if (userlib_strcmp(command, "tls_test") == 0) return tls_tools_test_main(argc - 1, argv + 1);
    }

    userlib_print_error("tls_tools: expected https, fetch, or tls_test");
    return 1;
}
