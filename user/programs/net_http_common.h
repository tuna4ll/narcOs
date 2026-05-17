#ifndef USER_PROGRAM_NET_HTTP_COMMON_H
#define USER_PROGRAM_NET_HTTP_COMMON_H

#include "program_utils.h"

#ifdef PROGRAM_ENABLE_TLS
#include "user_tls.h"
#endif

#define PROG_HTTP_IDLE_TIMEOUT 300U
#define PROG_HTTP_TOTAL_TIMEOUT 1200U

static int prog_parse_http_target(const char* target, char* host, int host_len,
                                  char* path, int path_len) {
    const char* cursor = prog_skip_spaces(target);
    int host_off = 0;
    int path_off = 0;

    if (!cursor || !host || !path || host_len <= 1 || path_len <= 1) return -1;
    if (prog_starts_with(cursor, "http://")) cursor += 7;
    else if (prog_starts_with(cursor, "https://")) cursor += 8;

    while (*cursor && *cursor != ' ' && *cursor != '/') {
        if (host_off + 1 >= host_len) return -1;
        host[host_off++] = *cursor++;
    }
    host[host_off] = '\0';
    if (host_off == 0) return -1;

    if (*cursor == '/') {
        while (*cursor && *cursor != ' ') {
            if (path_off + 1 >= path_len) return -1;
            path[path_off++] = *cursor++;
        }
    } else {
        cursor = prog_skip_spaces(cursor);
        if (*cursor != '\0') {
            if (*cursor != '/') {
                if (path_off + 1 >= path_len) return -1;
                path[path_off++] = '/';
            }
            while (*cursor) {
                if (path_off + 1 >= path_len) return -1;
                path[path_off++] = *cursor++;
            }
        }
    }

    if (path_off == 0) path[path_off++] = '/';
    path[path_off] = '\0';
    return 0;
}

static int prog_parse_fetch_args(const char* args, char* host, int host_len,
                                 char* path, int path_len, char* output_path, int output_path_len,
                                 int* out_use_https) {
    char first[128];
    char second[128];
    char third[128];
    const char* tail0;
    const char* tail1;

    if (out_use_https) *out_use_https = 0;
    if (prog_copy_token(args, first, sizeof(first)) != 0) return -1;
    tail0 = prog_find_arg_tail(args);
    if (prog_copy_token(tail0, second, sizeof(second)) != 0) return -1;
    tail1 = prog_find_arg_tail(tail0);

    if (*tail1 == '\0') {
        if (prog_starts_with(first, "https://") && out_use_https) *out_use_https = 1;
        if (prog_parse_http_target(first, host, host_len, path, path_len) != 0) return -1;
        userlib_strncpy(output_path, second, (size_t)output_path_len - 1U);
        output_path[output_path_len - 1] = '\0';
        return 0;
    }

    if (prog_copy_token(tail1, third, sizeof(third)) != 0) return -1;
    if (prog_find_arg_tail(tail1)[0] != '\0') return -1;

    {
        const char* host_token = first;
        if (prog_starts_with(host_token, "http://")) host_token += 7;
        else if (prog_starts_with(host_token, "https://")) {
            host_token += 8;
            if (out_use_https) *out_use_https = 1;
        }
        userlib_strncpy(host, host_token, (size_t)host_len - 1U);
    }
    host[host_len - 1] = '\0';
    userlib_strncpy(path, second, (size_t)path_len - 1U);
    path[path_len - 1] = '\0';
    if (path[0] != '/') return -1;
    userlib_strncpy(output_path, third, (size_t)output_path_len - 1U);
    output_path[output_path_len - 1] = '\0';
    return 0;
}

static int prog_append_http_text(char* dst, uint16_t dst_len, uint16_t* io_off, const char* src) {
    uint16_t off;

    if (!dst || !io_off || !src || dst_len == 0U) return NET_ERR_INVALID;
    off = *io_off;
    while (*src) {
        if (off + 1U >= dst_len) return NET_ERR_OVERFLOW;
        dst[off++] = *src++;
    }
    dst[off] = '\0';
    *io_off = off;
    return NET_OK;
}

static int prog_build_http_get_request(const char* host, const char* path,
                                       char* request, uint16_t request_cap,
                                       uint16_t* out_request_len) {
    uint16_t off = 0;

    if (!host || !path || !request || request_cap < 2U || !out_request_len) return NET_ERR_INVALID;
    request[0] = '\0';
    if (prog_append_http_text(request, request_cap, &off, "GET ") != NET_OK ||
        prog_append_http_text(request, request_cap, &off, path) != NET_OK ||
        prog_append_http_text(request, request_cap, &off, " HTTP/1.0\r\nHost: ") != NET_OK ||
        prog_append_http_text(request, request_cap, &off, host) != NET_OK ||
        prog_append_http_text(request, request_cap, &off,
                              "\r\nUser-Agent: NarcOs-Ring3/0.1\r\nConnection: close\r\n\r\n") != NET_OK) {
        return NET_ERR_OVERFLOW;
    }
    *out_request_len = (uint16_t)userlib_strlen(request);
    return NET_OK;
}

static void prog_http_finalize_response(char* response, uint16_t response_off,
                                        net_http_result_t* out_result) {
    if (!response || !out_result) return;
    response[response_off] = '\0';
    out_result->response_len = response_off;
}

static int prog_http_fetch_text(const char* host, const char* path,
                                char* response, uint16_t response_cap,
                                net_http_result_t* out_result) {
    char request[512];
    char discard[128];
    uint16_t request_len;
    uint16_t response_off = 0;
    uint32_t server_ip = 0;
    uint32_t started;
    uint32_t last_progress;
    int socket_handle;
    int status;

    if (!host || !path || !response || response_cap < 2U || !out_result) return NET_ERR_INVALID;
    userlib_memset(out_result, 0, sizeof(*out_result));
    response[0] = '\0';

    if (user_net_resolve(host, &server_ip) != NET_OK) return NET_ERR_RESOLVE;
    if (server_ip == 0U) return NET_ERR_RESOLVE;
    out_result->resolved_ip = server_ip;

    status = prog_build_http_get_request(host, path, request, sizeof(request), &request_len);
    if (status != NET_OK) return status;

    socket_handle = user_net_socket(NET_SOCK_STREAM);
    if (socket_handle < 0) return socket_handle;

    status = user_net_connect(socket_handle, server_ip, 80, NET_TIMEOUT_CONNECT_DEFAULT);
    if (status < 0) {
        (void)user_net_close(socket_handle);
        return status;
    }

    status = user_net_send(socket_handle, request, request_len);
    if (status < 0) {
        (void)user_net_close(socket_handle);
        return status;
    }
    if ((uint16_t)status != request_len) {
        (void)user_net_close(socket_handle);
        return NET_ERR_IO;
    }

    started = user_uptime_ticks();
    last_progress = started;
    while ((user_uptime_ticks() - started) <= PROG_HTTP_TOTAL_TIMEOUT) {
        int available = user_net_available(socket_handle);

        if (available == NET_ERR_CLOSED) {
            out_result->complete = 1U;
            break;
        }
        if (available < 0) {
            if (response_off == 0U) {
                (void)user_net_close(socket_handle);
                return available;
            }
            break;
        }
        if (available > 0) {
            int capacity = (int)(response_cap - 1U - response_off);
            void* target_buf = response + response_off;
            int to_read = available;
            int got;

            if (capacity <= 0) {
                target_buf = discard;
                to_read = available < (int)sizeof(discard) ? available : (int)sizeof(discard);
                out_result->truncated = 1U;
            } else if (to_read > capacity) {
                to_read = capacity;
                out_result->truncated = 1U;
            }

            got = user_net_recv(socket_handle, target_buf, (uint16_t)to_read);
            if (got < 0) {
                if (response_off == 0U) {
                    (void)user_net_close(socket_handle);
                    return got;
                }
                break;
            }
            if (got > 0) {
                if (target_buf == (void*)(response + response_off)) {
                    response_off = (uint16_t)(response_off + (uint16_t)got);
                }
                last_progress = user_uptime_ticks();
            }
        }
        if (user_uptime_ticks() > last_progress + PROG_HTTP_IDLE_TIMEOUT) break;
        user_yield();
    }

    prog_http_finalize_response(response, response_off, out_result);
    status = user_net_close(socket_handle);
    if (status < 0 && status != NET_ERR_TIMEOUT && response_off == 0U) return status;
    return response_off != 0U ? NET_OK : NET_ERR_TIMEOUT;
}

#ifdef PROGRAM_ENABLE_TLS
static int prog_https_fetch_text(const char* host, const char* path,
                                 char* response, uint16_t response_cap,
                                 net_http_result_t* out_result) {
    char request[512];
    char discard[128];
    uint16_t request_len = 0;
    uint16_t response_off = 0;
    uint32_t server_ip = 0;
    uint32_t started;
    uint32_t last_progress;
    int status;

    if (!host || !path || !response || response_cap < 2U || !out_result) return NET_ERR_INVALID;
    userlib_memset(out_result, 0, sizeof(*out_result));
    response[0] = '\0';

    if (user_net_resolve(host, &server_ip) != NET_OK) return NET_ERR_RESOLVE;
    out_result->resolved_ip = server_ip;

    status = prog_build_http_get_request(host, path, request, sizeof(request), &request_len);
    if (status != NET_OK) return status;

    status = user_tls_open(host);
    if (status != NET_OK) return status;

    status = user_tls_send(request, request_len);
    if (status < 0) {
        (void)user_tls_close();
        return status;
    }
    if ((uint16_t)status != request_len) {
        (void)user_tls_close();
        return NET_ERR_IO;
    }

    started = user_uptime_ticks();
    last_progress = started;
    while ((user_uptime_ticks() - started) <= PROG_HTTP_TOTAL_TIMEOUT) {
        int capacity = (int)(response_cap - 1U - response_off);
        void* target_buf = response + response_off;
        uint32_t to_read = capacity > 0 ? (uint32_t)capacity : (uint32_t)sizeof(discard);
        int got;

        if (capacity <= 0) {
            target_buf = discard;
            out_result->truncated = 1U;
        }

        got = user_tls_recv(target_buf, to_read);
        if (got == NET_ERR_CLOSED) {
            out_result->complete = 1U;
            break;
        }
        if (got < 0) {
            if (response_off == 0U) {
                (void)user_tls_close();
                return got;
            }
            break;
        }
        if (got > 0) {
            if (target_buf == (void*)(response + response_off)) {
                response_off = (uint16_t)(response_off + (uint16_t)got);
            } else {
                out_result->truncated = 1U;
            }
            last_progress = user_uptime_ticks();
            continue;
        }
        if (user_uptime_ticks() > last_progress + PROG_HTTP_IDLE_TIMEOUT) break;
        user_yield();
    }

    prog_http_finalize_response(response, response_off, out_result);
    status = user_tls_close();
    if (status < 0 && status != NET_ERR_TIMEOUT && status != NET_ERR_CLOSED &&
        status != USER_TLS_ERR_ALERT && response_off == 0U) {
        return status;
    }
    return response_off != 0U ? NET_OK : NET_ERR_TIMEOUT;
}
#endif

static uint32_t prog_http_find_body(const char* response, uint32_t length) {
    for (uint32_t i = 0; i + 3U < length; i++) {
        if (response[i] == '\r' && response[i + 1U] == '\n' &&
            response[i + 2U] == '\r' && response[i + 3U] == '\n') {
            return i + 4U;
        }
    }
    for (uint32_t i = 0; i + 1U < length; i++) {
        if (response[i] == '\n' && response[i + 1U] == '\n') return i + 2U;
    }
    return 0;
}

static int prog_print_http_response(const char* target, const char* response,
                                    const net_http_result_t* result) {
    char line[224];
    int off = 0;

    line[0] = '\0';
    if (prog_append_text(line, sizeof(line), &off, "HTTP GET        : ") != 0) return -1;
    if (prog_append_text(line, sizeof(line), &off, target) != 0) return -1;
    if (userlib_println(line) != 0) return -1;

    line[0] = '\0';
    off = 0;
    if (prog_append_text(line, sizeof(line), &off, "Resolved        : ") != 0) return -1;
    if (prog_append_ip(line, sizeof(line), &off, result->resolved_ip) != 0) return -1;
    if (userlib_println(line) != 0) return -1;

    if (userlib_println("---- response ----") != 0) return -1;
    if (userlib_println((response && response[0] != '\0') ? response : "(empty response)") != 0) return -1;
    if (result->truncated != 0U && userlib_println("warning: Response truncated to local buffer size.") != 0) return -1;
    if (result->complete == 0U && userlib_println("warning: Remote peer did not close cleanly before timeout.") != 0) return -1;
    return 0;
}

#endif
