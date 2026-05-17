#include "program_utils.h"

int main(void) {
    disk_fs_node_t entries[MAX_FILES];
    int count = user_fs_list(entries, MAX_FILES);

    if (count < 0) {
        userlib_print_error("ls: failed to read directory");
        return 1;
    }

    if (userlib_println("Name\t\tSize (Bytes)") != 0) return 1;
    if (userlib_println("----------------------------") != 0) return 1;
    for (int i = 0; i < count; i++) {
        char line[96];
        int off = 0;

        line[0] = '\0';
        if (prog_append_text(line, sizeof(line), &off, entries[i].name) != 0) return 1;
        if (entries[i].flags == FS_NODE_DIR) {
            if (prog_append_text(line, sizeof(line), &off, "/\t\t<DIR>") != 0) return 1;
        } else {
            if (prog_append_char(line, sizeof(line), &off, '\t') != 0) return 1;
            if (userlib_strlen(entries[i].name) < 8U &&
                prog_append_char(line, sizeof(line), &off, '\t') != 0) {
                return 1;
            }
            if (prog_append_uint(line, sizeof(line), &off, entries[i].size) != 0) return 1;
        }
        if (userlib_println(line) != 0) return 1;
    }
    return 0;
}
