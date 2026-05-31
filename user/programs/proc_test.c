#include "user_lib.h"
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

static int proc_test_child_main(void) {
    struct timespec delay;

    delay.tv_sec = 0;
    delay.tv_nsec = 100000000;
    (void)nanosleep(&delay, 0);
    return 42;
}

int main(int argc, char** argv) {
    static const char* child_argv[] = { "proc_test", "child", 0 };
    int pid;
    int status = 0;
    int rc;

    if (argc > 1 && strcmp(argv[1], "child") == 0) {
        return proc_test_child_main();
    }

    pid = user_spawn("/bin/proc_test", child_argv, 2U);
    if (pid < 0) {
        fputs("proc_test: spawn failed\n", stderr);
        return 1;
    }

    rc = waitpid(pid, &status, WNOHANG);
    if (rc != 0) {
        fputs("proc_test: nohang failed\n", stderr);
        return 1;
    }

    rc = waitpid(pid, &status, 0);
    if (rc != pid || status != 42) {
        fputs("proc_test: wait returned wrong status\n", stderr);
        return 1;
    }

    rc = waitpid(pid, &status, WNOHANG);
    if (rc != -1) {
        fputs("proc_test: zombie reap check failed\n", stderr);
        return 1;
    }

    return puts("proc_test: ok") >= 0 ? 0 : 1;
}
