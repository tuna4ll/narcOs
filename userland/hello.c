#include <stdio.h>
#include <unistd.h>

int main(void) {
    for (int i = 0; i < 3; i++) {
        printf("Hello from process %d (%d)\n", getpid(), i + 1);
        for (volatile unsigned long spin = 0; spin < 50000000; spin++)
            __asm__ volatile ("" ::: "memory");
    }
    return 0;
}
