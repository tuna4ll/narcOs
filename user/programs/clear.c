#include <stdio.h>

int main(void) {
    return fputs("\033[2J\033[H", stdout) >= 0 ? 0 : 1;
}
