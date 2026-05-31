#define _BSD_SOURCE

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int check(int condition, const char* message) {
    if (condition) return 0;
    fputs(message, stderr);
    fputs("\n", stderr);
    return -1;
}

static int same_bytes(const char* left, const char* right, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (left[i] != right[i]) return 0;
    }
    return 1;
}

int main(void) {
    static const char payload[] = "narcos posix smoke\n";
    char buffer[32];
    char cwd[128];
    struct timespec now;
    struct timespec nap;
    void* brk0;
    void* brk1;
    int fd;
    int dup_fd;
    int status = 0;
    FILE* stream;
    DIR* dir;
    struct dirent* ent;
    struct stat posix_stat;
    int saw_file = 0;

    fd = open("/posix_smoke.txt", O_CREAT | O_TRUNC | O_RDWR, 0644);
    if (check(fd >= 0, "openat failed") != 0) return 1;
    if (check(write(fd, payload, sizeof(payload) - 1U) == (ssize_t)(sizeof(payload) - 1U),
              "write failed") != 0) status = 1;
    if (check(lseek(fd, 0, SEEK_SET) == 0, "lseek failed") != 0) status = 1;
    memset(buffer, 0, sizeof(buffer));
    if (check(read(fd, buffer, sizeof(payload) - 1U) == (ssize_t)(sizeof(payload) - 1U),
              "read failed") != 0) status = 1;
    if (check(same_bytes(buffer, payload, sizeof(payload) - 1U), "read payload mismatch") != 0) status = 1;
    if (check(fstat(fd, &posix_stat) == 0 && posix_stat.st_size == (off_t)(sizeof(payload) - 1U),
              "fstat failed") != 0) status = 1;
    if (check(close(fd) == 0, "close failed") != 0) status = 1;
    if (check(stat("/posix_smoke.txt", &posix_stat) == 0 &&
                  posix_stat.st_size == (off_t)(sizeof(payload) - 1U),
              "stat failed") != 0) status = 1;

    brk0 = sbrk(0);
    brk1 = sbrk(4096);
    if (check(brk0 != (void*)-1 && brk1 == brk0, "sbrk failed") != 0) status = 1;
    if (brk0 != (void*)-1 && check(brk(brk0) == 0, "brk restore failed") != 0) status = 1;

    if (check(clock_gettime(CLOCK_REALTIME, &now) == 0 && now.tv_nsec >= 0,
              "clock_gettime failed") != 0) status = 1;
    nap.tv_sec = 0;
    nap.tv_nsec = 10000000;
    if (check(nanosleep(&nap, 0) == 0, "nanosleep failed") != 0) status = 1;

    remove("/posix_dir/renamed.txt");
    remove("/posix_dir/stdio.txt");
    remove("/posix_dir");
    if (check(mkdir("/posix_dir", 0755) == 0, "mkdir failed") != 0) status = 1;
    stream = fopen("/posix_dir/stdio.txt", "w+");
    if (check(stream != 0, "fopen failed") != 0) status = 1;
    else {
        if (check(fwrite("stdio ok", 1, 8, stream) == 8, "fwrite failed") != 0) status = 1;
        if (check(fseek(stream, 0, SEEK_SET) == 0, "fseek failed") != 0) status = 1;
        memset(buffer, 0, sizeof(buffer));
        if (check(fread(buffer, 1, 8, stream) == 8, "fread failed") != 0) status = 1;
        if (check(same_bytes(buffer, "stdio ok", 8), "stdio payload mismatch") != 0) status = 1;
        if (check(fclose(stream) == 0, "fclose failed") != 0) status = 1;
    }
    if (check(rename("/posix_dir/stdio.txt", "/posix_dir/renamed.txt") == 0, "rename failed") != 0) status = 1;
    if (check(stat("/posix_dir/renamed.txt", &posix_stat) == 0 && posix_stat.st_size == 8,
              "posix stat failed") != 0) status = 1;
    if (check(fstatat(AT_FDCWD, "/posix_dir/renamed.txt", &posix_stat, 0) == 0,
              "fstatat failed") != 0) status = 1;
    if (check(access("/posix_dir/renamed.txt", F_OK) == 0, "access failed") != 0) status = 1;

    dir = opendir("/posix_dir");
    if (check(dir != 0, "opendir failed") != 0) status = 1;
    else {
        while ((ent = readdir(dir)) != 0) {
            if (strcmp(ent->d_name, "renamed.txt") == 0) saw_file = 1;
        }
        if (check(saw_file, "readdir did not find file") != 0) status = 1;
        if (check(closedir(dir) == 0, "closedir failed") != 0) status = 1;
    }

    fd = open("/posix_dir/renamed.txt", O_RDONLY);
    if (check(fd >= 0, "posix open failed") != 0) status = 1;
    else {
        dup_fd = dup(fd);
        if (check(dup_fd >= 0, "dup failed") != 0) status = 1;
        if (dup_fd >= 0) {
            if (check(fcntl(dup_fd, F_GETFD) >= 0, "fcntl F_GETFD failed") != 0) status = 1;
            close(dup_fd);
        }
        close(fd);
    }
    if (check(isatty(STDOUT_FILENO) == 1, "isatty stdout failed") != 0) status = 1;
    if (check(getcwd(cwd, sizeof(cwd)) != 0 && cwd[0] == '/', "getcwd failed") != 0) status = 1;
    if (check(chdir("/posix_dir") == 0, "chdir failed") != 0) status = 1;
    if (check(getcwd(cwd, sizeof(cwd)) != 0 && strcmp(cwd, "/posix_dir") == 0,
              "chdir cwd mismatch") != 0) status = 1;
    (void)chdir("/");
    if (check(unlink("/posix_dir/renamed.txt") == 0, "unlink failed") != 0) status = 1;
    if (check(rmdir("/posix_dir") == 0, "rmdir failed") != 0) status = 1;

    if (status == 0) puts("posix smoke ok");
    return status;
}
