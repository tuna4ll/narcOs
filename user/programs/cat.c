#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define CAT_CHUNK_SIZE 256

static int cat_stream_stdin(void) {
    char buffer[CAT_CHUNK_SIZE];

    for (;;) {
        ssize_t rc = read(STDIN_FILENO, buffer, sizeof(buffer));
        if (rc < 0) {
            fputs("cat: stdin read failed\n", stderr);
            return 1;
        }
        if (rc == 0) return 0;
        if (write(STDOUT_FILENO, buffer, (size_t)rc) != rc) return 1;
    }
}

static int cat_file(const char* path) {
    char buffer[CAT_CHUNK_SIZE];
    int fd = open(path, O_RDONLY);

    if (fd < 0) {
        fputs("cat: open failed\n", stderr);
        return 1;
    }

    for (;;) {
        ssize_t rc = read(fd, buffer, sizeof(buffer));

        if (rc < 0) {
            fputs("cat: read failed\n", stderr);
            close(fd);
            return 1;
        }
        if (rc == 0) break;
        if (write(STDOUT_FILENO, buffer, (size_t)rc) != rc) {
            close(fd);
            return 1;
        }
    }
    return close(fd) == 0 ? 0 : 1;
}

int main(int argc, char** argv) {
    int exit_code = 0;

    if (argc == 1) return cat_stream_stdin();
    for (int i = 1; i < argc; i++) {
        if (cat_file(argv[i]) != 0) exit_code = 1;
    }
    return exit_code;
}
