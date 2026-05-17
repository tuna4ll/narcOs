#ifndef USER_LIB_H
#define USER_LIB_H

#include <stddef.h>
#include <stdint.h>
#include "user_abi.h"

#define USER_STDIN 0
#define USER_STDOUT 1
#define USER_STDERR 2

static inline size_t userlib_strlen(const char* text) {
    size_t len = 0;

    if (!text) return 0;
    while (text[len] != '\0') len++;
    return len;
}

static inline int userlib_strcmp(const char* left, const char* right) {
    while (*left && (*left == *right)) {
        left++;
        right++;
    }
    return *(const unsigned char*)left - *(const unsigned char*)right;
}

static inline int userlib_strncmp(const char* left, const char* right, size_t len) {
    while (len != 0U && *left && (*left == *right)) {
        left++;
        right++;
        len--;
    }
    if (len == 0U) return 0;
    return *(const unsigned char*)left - *(const unsigned char*)right;
}

static inline char* userlib_strncpy(char* dest, const char* src, size_t len) {
    size_t i = 0;

    for (; i < len && src[i] != '\0'; i++) dest[i] = src[i];
    for (; i < len; i++) dest[i] = '\0';
    return dest;
}

static inline void* userlib_memset(void* dest, int value, size_t len) {
    unsigned char* out = (unsigned char*)dest;

    while (len-- != 0U) *out++ = (unsigned char)value;
    return dest;
}

static inline void* userlib_memcpy(void* dest, const void* src, size_t len) {
    unsigned char* out = (unsigned char*)dest;
    const unsigned char* in = (const unsigned char*)src;

    while (len-- != 0U) *out++ = *in++;
    return dest;
}

static inline uint32_t userlib_format_u32(char* out, uint32_t value) {
    char digits[16];
    uint32_t count = 0;

    if (!out) return 0;
    if (value == 0U) {
        out[0] = '0';
        return 1U;
    }
    while (value != 0U && count < (uint32_t)sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    for (uint32_t i = 0; i < count; i++) {
        out[i] = digits[count - i - 1U];
    }
    return count;
}

static inline int userlib_write_all(int fd, const void* data, uint32_t len) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t written = 0;

    while (written < len) {
        int rc = user_write(fd, bytes + written, len - written);
        if (rc <= 0) return -1;
        written += (uint32_t)rc;
    }
    return 0;
}

static inline int userlib_print_fd(int fd, const char* text) {
    return userlib_write_all(fd, text, (uint32_t)userlib_strlen(text));
}

static inline int userlib_print(const char* text) {
    return userlib_print_fd(USER_STDOUT, text);
}

static inline int userlib_println(const char* text) {
    char line[256];
    size_t len;

    if (!text) text = "";
    len = userlib_strlen(text);
    if (len + 1U < sizeof(line)) {
        userlib_memcpy(line, text, len);
        line[len] = '\n';
        return userlib_write_all(USER_STDOUT, line, (uint32_t)(len + 1U));
    }
    if (userlib_print_fd(USER_STDOUT, text) != 0) return -1;
    return userlib_write_all(USER_STDOUT, "\n", 1U);
}

static inline int userlib_print_error(const char* text) {
    char line[256];
    size_t len;

    if (!text) text = "";
    len = userlib_strlen(text);
    if (len + 1U < sizeof(line)) {
        userlib_memcpy(line, text, len);
        line[len] = '\n';
        return userlib_write_all(USER_STDERR, line, (uint32_t)(len + 1U));
    }
    if (userlib_print_fd(USER_STDERR, text) != 0) return -1;
    return userlib_write_all(USER_STDERR, "\n", 1U);
}

static inline int userlib_print_u32_fd(int fd, uint32_t value) {
    char digits[16];
    uint32_t len = userlib_format_u32(digits, value);

    return userlib_write_all(fd, digits, len);
}

static inline int userlib_print_i32_fd(int fd, int32_t value) {
    char digits[17];
    uint32_t len;
    uint32_t magnitude;

    if (value < 0) {
        magnitude = (uint32_t)(-(value + 1)) + 1U;
        digits[0] = '-';
        len = userlib_format_u32(digits + 1, magnitude);
        return userlib_write_all(fd, digits, len + 1U);
    }
    return userlib_print_u32_fd(fd, (uint32_t)value);
}

static inline int userlib_parse_i32(const char* text, int* out_value) {
    int sign = 1;
    int value = 0;

    if (!text || !out_value || *text == '\0') return -1;
    if (*text == '-') {
        sign = -1;
        text++;
    }
    if (*text < '0' || *text > '9') return -1;
    while (*text >= '0' && *text <= '9') {
        value = value * 10 + (*text - '0');
        text++;
    }
    if (*text != '\0') return -1;
    *out_value = value * sign;
    return 0;
}

static inline int userlib_min_int(int left, int right) {
    return left < right ? left : right;
}

#endif
