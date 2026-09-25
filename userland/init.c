#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <string.h>
#include <unistd.h>

static int write_all(int fd, const void *data, size_t length) {
    const unsigned char *bytes = data;
    while (length) {
        ssize_t count = write(fd, bytes, length);
        if (count <= 0) return -1;
        bytes += (size_t)count;
        length -= (size_t)count;
    }
    return 0;
}

int main(int argc, char **argv, char **envp) {
    (void)envp;
    static const char banner[] = "narcOs init\nlibc: posix layer ready\n";
    if (argc != 1 || !argv || strcmp(argv[0], "/sbin/init") != 0) return 1;
    if (write_all(STDOUT_FILENO, banner, strlen(banner)) != 0) return 2;
    if (getpid() <= 0) return 3;

    int fd = open("/etc/motd", O_RDONLY);
    if (fd < 0) return 4;
    unsigned char buffer[64];
    ssize_t count = read(fd, buffer, sizeof(buffer));
    if (count < 0) return 5;
    static const char prefix[] = "motd: ";
    if (write_all(STDOUT_FILENO, prefix, sizeof(prefix) - 1) != 0 ||
        write_all(STDOUT_FILENO, buffer, (size_t)count) != 0)
        return 6;
    if (lseek(fd, 0, SEEK_SET) != 0 || close(fd) != 0) return 7;

    errno = 0;
    if (open("/missing", O_RDONLY) != -1 || errno != ENOENT) return 8;
    if (sched_yield() != 0) return 9;
    return 0;
}
