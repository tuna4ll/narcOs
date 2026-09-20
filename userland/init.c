#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
    puts("narcOs init");

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
