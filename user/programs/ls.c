#include <dirent.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>

static int append_char(char* dst, int dst_len, int* io_off, char c) {
    int off;

    if (!dst || !io_off || dst_len <= 0) return -1;
    off = *io_off;
    if (off + 1 >= dst_len) return -1;
    dst[off++] = c;
    dst[off] = '\0';
    *io_off = off;
    return 0;
}

static int append_text(char* dst, int dst_len, int* io_off, const char* src) {
    if (!src) src = "";
    while (*src) {
        if (append_char(dst, dst_len, io_off, *src++) != 0) return -1;
    }
    return 0;
}

static int append_uint(char* dst, int dst_len, int* io_off, uint32_t value) {
    char digits[16];
    int digit_count = 0;

    if (value == 0U) return append_char(dst, dst_len, io_off, '0');
    while (value != 0U && digit_count < (int)sizeof(digits)) {
        digits[digit_count++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    for (int i = digit_count - 1; i >= 0; i--) {
        if (append_char(dst, dst_len, io_off, digits[i]) != 0) return -1;
    }
    return 0;
}

static int print_file_line(const char* name, long size) {
    char line[128];
    int off = 0;

    line[0] = '\0';
    if (append_text(line, sizeof(line), &off, name) != 0) return -1;
    if (append_text(line, sizeof(line), &off, name && name[7] == '\0' ? "\t\t" : "\t") != 0) return -1;
    if (append_uint(line, sizeof(line), &off, size > 0 ? (uint32_t)size : 0U) != 0) return -1;
    return puts(line) >= 0 ? 0 : -1;
}

static int print_dir_line(const char* name) {
    char line[128];
    int off = 0;

    line[0] = '\0';
    if (append_text(line, sizeof(line), &off, name) != 0) return -1;
    if (append_text(line, sizeof(line), &off, "/\t\t<DIR>") != 0) return -1;
    return puts(line) >= 0 ? 0 : -1;
}

int main(void) {
    DIR* dir = opendir(".");
    struct dirent* ent;

    if (!dir) {
        fputs("ls: failed to read directory\n", stderr);
        return 1;
    }

    if (puts("Name\t\tSize (Bytes)") < 0 || puts("----------------------------") < 0) {
        closedir(dir);
        return 1;
    }
    while ((ent = readdir(dir)) != 0) {
        struct stat st;

        if (stat(ent->d_name, &st) == 0 && S_ISDIR(st.st_mode)) {
            if (print_dir_line(ent->d_name) != 0) {
                closedir(dir);
                return 1;
            }
        } else {
            if (print_file_line(ent->d_name, stat(ent->d_name, &st) == 0 ? st.st_size : 0) != 0) {
                closedir(dir);
                return 1;
            }
        }
    }
    return closedir(dir) == 0 ? 0 : 1;
}
