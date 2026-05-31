#include <stdio.h>

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1 && putchar(' ') == EOF) return 1;
        if (fputs(argv[i] ? argv[i] : "", stdout) == EOF) return 1;
    }
    return putchar('\n') == EOF ? 1 : 0;
}
