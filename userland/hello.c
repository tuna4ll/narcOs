#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    puts("Hello from userspace");

    char *message = malloc(64);
    if (!message) {
        puts("[musl] malloc failed");
        return 1;
    }

    strcpy(message, "malloc + string are alive");
    printf("[musl] %s\n", message);
    free(message);

    puts("[musl] stdio + malloc + string OK");
    return 0;
}
