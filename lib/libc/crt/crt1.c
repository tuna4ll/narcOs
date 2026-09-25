#include <stdlib.h>

extern int main(int argc, char **argv, char **envp);

_Noreturn void __libc_start(int argc, char **argv, char **envp) {
    exit(main(argc, argv, envp));
}
