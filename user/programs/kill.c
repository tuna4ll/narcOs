#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    int exit_code = 0;

    if (argc < 2) {
        fputs("Usage: kill <pid> [pid...]\n", stderr);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        char* end = 0;
        long pid = strtol(argv[i], &end, 10);

        if (!argv[i] || argv[i][0] == '\0' || !end || *end != '\0' || pid <= 0) {
            fputs("kill: invalid pid\n", stderr);
            exit_code = 1;
            continue;
        }
        if (kill((pid_t)pid, SIGTERM) != 0) {
            fputs("kill: syscall failed\n", stderr);
            exit_code = 1;
        }
    }

    return exit_code;
}
