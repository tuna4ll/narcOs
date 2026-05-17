#include "user_lib.h"

int main(void) {
    char path[128];

    if (user_fs_get_cwd(path, sizeof(path)) != 0) {
        userlib_print_error("pwd: failed to get current path");
        return 1;
    }
    return userlib_println(path) == 0 ? 0 : 1;
}
