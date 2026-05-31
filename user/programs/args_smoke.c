#include "user_lib.h"
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int expect_arg(int argc, char** argv, int index, const char* expected) {
    if (index >= argc || !argv || !argv[index] || strcmp(argv[index], expected) != 0) {
        fputs("args_smoke: argv mismatch\n", stderr);
        return -1;
    }
    return 0;
}

static int child_main(int argc, char** argv, const char* mode) {
    if (argc != 5) {
        fputs("args_smoke: argc mismatch\n", stderr);
        return 1;
    }
    if (expect_arg(argc, argv, 0, "/bin/args_smoke") != 0) return 1;
    if (expect_arg(argc, argv, 1, mode) != 0) return 1;
    if (expect_arg(argc, argv, 2, "one") != 0) return 1;
    if (expect_arg(argc, argv, 3, "two words") != 0) return 1;
    if (expect_arg(argc, argv, 4, "seven") != 0) return 1;
    return 0;
}

int main(int argc, char** argv) {
    static const char* spawn_argv[] = { "/bin/args_smoke", "spawn-child", "one", "two words", "seven" };
    static const char* exec_argv[] = { "/bin/args_smoke", "exec-child", "one", "two words", "seven", 0 };
    int pid;
    int status = -1;

    if (argc >= 2 && strcmp(argv[1], "spawn-child") == 0) return child_main(argc, argv, "spawn-child");
    if (argc >= 2 && strcmp(argv[1], "exec-child") == 0) return child_main(argc, argv, "exec-child");

    if (argc != 1 || !argv || !argv[0]) {
        fputs("args_smoke: initial argc/argv invalid\n", stderr);
        return 1;
    }

    pid = user_spawn("/bin/args_smoke", spawn_argv, 5U);
    if (pid < 0 || waitpid(pid, &status, 0) != pid || status != 0) {
        fputs("args_smoke: spawn argv failed\n", stderr);
        return 1;
    }

    if (execve("/bin/args_smoke", (char* const*)exec_argv, 0) != 0) {
        fputs("args_smoke: exec argv failed\n", stderr);
        return 1;
    }
    return 1;
}
