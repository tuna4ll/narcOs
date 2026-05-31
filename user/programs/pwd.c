#include <stdio.h>
#include <unistd.h>

int main(void) {
    char path[128];

    if (!getcwd(path, sizeof(path))) {
        fputs("pwd: failed to get current path\n", stderr);
        return 1;
    }
    return puts(path) >= 0 ? 0 : 1;
}
