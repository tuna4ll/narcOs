#ifndef USER_PROGRAM_UTILS_H
#define USER_PROGRAM_UTILS_H

#include "user_lib.h"

static int prog_starts_with(const char* text, const char* prefix) {
    return userlib_strncmp(text, prefix, userlib_strlen(prefix)) == 0;
}

static const char* prog_skip_spaces(const char* text) {
    while (text && *text == ' ') text++;
    return text ? text : "";
}

static const char* prog_find_arg_tail(const char* text) {
    text = prog_skip_spaces(text);
    while (*text && *text != ' ') text++;
    return prog_skip_spaces(text);
}

static int prog_copy_token(const char* src, char* out, int out_len) {
    int len = 0;

    if (!src || !out || out_len <= 1) return -1;
    src = prog_skip_spaces(src);
    if (*src == '\0') {
        out[0] = '\0';
        return -1;
    }
    while (*src && *src != ' ') {
        if (len + 1 >= out_len) return -1;
        out[len++] = *src++;
    }
    out[len] = '\0';
    return 0;
}

static int prog_copy_remainder(const char* src, char* out, int out_len) {
    int len = 0;

    if (!src || !out || out_len <= 0) return -1;
    src = prog_skip_spaces(src);
    while (*src) {
        if (len + 1 >= out_len) return -1;
        out[len++] = *src++;
    }
    out[len] = '\0';
    return len > 0 ? 0 : -1;
}

static int prog_join_args(int argc, char** argv, int start, char* out, int out_len) {
    int off = 0;

    if (!out || out_len <= 0) return -1;
    out[0] = '\0';
    for (int i = start; i < argc; i++) {
        const char* arg = argv[i] ? argv[i] : "";

        if (i > start) {
            if (off + 1 >= out_len) return -1;
            out[off++] = ' ';
        }
        while (*arg) {
            if (off + 1 >= out_len) return -1;
            out[off++] = *arg++;
        }
    }
    out[off] = '\0';
    return off > 0 ? 0 : -1;
}

static int prog_append_char(char* dst, int dst_len, int* io_off, char c) {
    int off;

    if (!dst || !io_off || dst_len <= 0) return -1;
    off = *io_off;
    if (off + 1 >= dst_len) return -1;
    dst[off++] = c;
    dst[off] = '\0';
    *io_off = off;
    return 0;
}

static int prog_append_text(char* dst, int dst_len, int* io_off, const char* src) {
    if (!src) src = "";
    while (*src) {
        if (prog_append_char(dst, dst_len, io_off, *src++) != 0) return -1;
    }
    return 0;
}

static int prog_append_uint(char* dst, int dst_len, int* io_off, uint32_t value) {
    char digits[16];
    int digit_count = 0;

    if (value == 0U) return prog_append_char(dst, dst_len, io_off, '0');
    while (value != 0U && digit_count < (int)sizeof(digits)) {
        digits[digit_count++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    for (int i = digit_count - 1; i >= 0; i--) {
        if (prog_append_char(dst, dst_len, io_off, digits[i]) != 0) return -1;
    }
    return 0;
}

static int prog_append_i32(char* dst, int dst_len, int* io_off, int32_t value) {
    uint32_t magnitude;

    if (value < 0) {
        if (prog_append_char(dst, dst_len, io_off, '-') != 0) return -1;
        magnitude = (uint32_t)(-(value + 1)) + 1U;
        return prog_append_uint(dst, dst_len, io_off, magnitude);
    }
    return prog_append_uint(dst, dst_len, io_off, (uint32_t)value);
}

static int prog_append_ip(char* dst, int dst_len, int* io_off, uint32_t ip_addr) {
    for (int i = 0; i < 4; i++) {
        uint32_t octet = (ip_addr >> (24 - i * 8)) & 0xFFU;

        if (prog_append_uint(dst, dst_len, io_off, octet) != 0) return -1;
        if (i != 3 && prog_append_char(dst, dst_len, io_off, '.') != 0) return -1;
    }
    return 0;
}

static int prog_print_ip(uint32_t ip_addr) {
    char line[24];
    int off = 0;

    line[0] = '\0';
    if (prog_append_ip(line, sizeof(line), &off, ip_addr) != 0) return -1;
    return userlib_print(line);
}

static int prog_print_ip_line(const char* label, uint32_t ip_addr) {
    char line[96];
    int off = 0;

    line[0] = '\0';
    if (prog_append_text(line, sizeof(line), &off, label) != 0) return -1;
    if (prog_append_ip(line, sizeof(line), &off, ip_addr) != 0) return -1;
    return userlib_println(line);
}

static const char* prog_net_error_string(int status) {
    switch (status) {
        case NET_ERR_INVALID: return "invalid request";
        case NET_ERR_UNSUPPORTED: return "unsupported";
        case NET_ERR_NOT_READY: return "network not ready";
        case NET_ERR_NO_SOCKETS: return "no sockets available";
        case NET_ERR_RESOLVE: return "dns resolve failed";
        case NET_ERR_TIMEOUT: return "timeout";
        case NET_ERR_STATE: return "invalid socket state";
        case NET_ERR_RESET: return "connection reset";
        case NET_ERR_CLOSED: return "connection closed";
        case NET_ERR_WOULD_BLOCK: return "would block";
        case NET_ERR_IO: return "i/o error";
        case NET_ERR_OVERFLOW: return "buffer overflow";
        default: return "unknown error";
    }
}

static int prog_print_two_digits(uint32_t value) {
    if (value < 10U && userlib_print("0") != 0) return -1;
    return userlib_print_u32_fd(USER_STDOUT, value);
}

static int prog_append_two_digits(char* dst, int dst_len, int* io_off, uint32_t value) {
    if (value < 10U && prog_append_char(dst, dst_len, io_off, '0') != 0) return -1;
    return prog_append_uint(dst, dst_len, io_off, value);
}

static int prog_format_datetime(char* out, int out_len, const rtc_local_time_t* t, int date_only) {
    int off = 0;

    if (!out || out_len <= 0 || !t) return -1;
    out[0] = '\0';
    if (prog_append_text(out, out_len, &off, "20") != 0) return -1;
    if (prog_append_two_digits(out, out_len, &off, t->year) != 0) return -1;
    if (prog_append_char(out, out_len, &off, '-') != 0) return -1;
    if (prog_append_two_digits(out, out_len, &off, t->month) != 0) return -1;
    if (prog_append_char(out, out_len, &off, '-') != 0) return -1;
    if (prog_append_two_digits(out, out_len, &off, t->day) != 0) return -1;
    if (date_only) return 0;
    if (prog_append_char(out, out_len, &off, ' ') != 0) return -1;
    if (prog_append_two_digits(out, out_len, &off, t->hour) != 0) return -1;
    if (prog_append_char(out, out_len, &off, ':') != 0) return -1;
    if (prog_append_two_digits(out, out_len, &off, t->minute) != 0) return -1;
    if (prog_append_char(out, out_len, &off, ':') != 0) return -1;
    return prog_append_two_digits(out, out_len, &off, t->second);
}

#endif
