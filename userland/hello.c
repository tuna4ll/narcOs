#include <sched.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    for (int i = 0; i < 3; i++) {
        printf("Hello from process %d (%d)\n", getpid(), i + 1);
        sched_yield();
    }
    return 0;
}
