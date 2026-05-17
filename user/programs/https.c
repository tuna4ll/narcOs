#include "user_lib.h"

int main(int argc, char** argv) {
    int status = user_exec("/bin/tls_tools", (const char* const*)argv, (uint32_t)argc);

    (void)status;
    userlib_print_error("https: failed to exec /bin/tls_tools");
    return 1;
}
