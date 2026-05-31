#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    (void)argc;
    (void)execve("/bin/tls_tools", argv, 0);
    fputs("tls_test: failed to exec /bin/tls_tools\n", stderr);
    return 1;
}
