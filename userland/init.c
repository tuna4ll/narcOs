#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <narcos/narc.h>

int main(void) {
    puts("narcOs init");

    narc_result_t abi = narc_abi_query();
    narc_result_t pid = narc_getpid();
    static const char native_message[] = "libnarc: native ABI ready\n";
    narc_result_t wrote = narc_write(1, native_message, sizeof(native_message) - 1);
    if (abi.status != NARC_OK || abi.value != (int64_t)NARC_ABI_VERSION ||
        pid.status != NARC_OK || pid.value <= 0 ||
        wrote.status != NARC_OK || wrote.value != (int64_t)(sizeof(native_message) - 1))
        return 8;

    static const char motd_path[] = "/etc/motd";
    narc_result_t native_fd = narc_open(motd_path, sizeof(motd_path) - 1, NARC_OPEN_READ);
    if (native_fd.status != NARC_OK) return 9;
    char native_buf[8];
    narc_result_t native_read = narc_read((int)native_fd.value, native_buf, sizeof(native_buf));
    narc_result_t native_seek = narc_seek((int)native_fd.value, 0, NARC_SEEK_BEGIN);
    narc_result_t native_close = narc_close((int)native_fd.value);
    if (native_read.status != NARC_OK || native_read.value != 7 ||
        native_seek.status != NARC_OK || native_seek.value != 0 ||
        native_close.status != NARC_OK)
        return 10;

    static const char missing_path[] = "/missing";
    narc_result_t missing = narc_open(missing_path, sizeof(missing_path) - 1, NARC_OPEN_READ);
    if (missing.status != NARC_NOT_FOUND) return 11;

    int fd = openat(AT_FDCWD, "/etc/motd", O_RDONLY);
    if (fd < 0) return 1;
    struct stat st;
    if (fstat(fd, &st) != 0) return 2;
    struct stat path_st;
    if (stat("/etc/motd", &path_st) != 0 || path_st.st_size != st.st_size) return 3;
    char buf[64];
    ssize_t len = read(fd, buf, sizeof(buf));
    if (len < 0) return 4;
    printf("motd (%lld bytes): %.*s", (long long)st.st_size, (int)len, buf);
    if (lseek(fd, 0, SEEK_SET) != 0 || close(fd) != 0) return 5;

    DIR *dir = opendir("/bin");
    if (!dir) return 6;
    puts("/bin:");
    struct dirent *entry;
    while ((entry = readdir(dir))) printf("  %s\n", entry->d_name);
    if (closedir(dir) != 0) return 7;
    return 0;
}
