#include "fd.h"
#include "fs.h"
#include "memory_alloc.h"
#include "serial.h"
#include "string.h"

extern void vga_write(const char* data, uint32_t len);
extern int console_input_read(char* buffer, uint32_t max_len);

static fd_handle_t* fd_create_console_handle(uint32_t access) {
    fd_handle_t* handle = (fd_handle_t*)malloc(sizeof(fd_handle_t));

    if (!handle) return 0;
    memset(handle, 0, sizeof(*handle));
    handle->kind = FD_KIND_CONSOLE;
    handle->refs = 1U;
    handle->access = access;
    return handle;
}

static fd_handle_t* fd_create_file_handle(int node_idx, uint32_t access, uint32_t open_flags) {
    disk_fs_node_t node;
    fd_handle_t* handle;

    if (node_idx < 0 || fs_get_node_info(node_idx, &node) != 0) return 0;
    if ((open_flags & FD_OPEN_DIRECTORY) != 0U) {
        if (node.flags != FS_NODE_DIR) return 0;
    } else if (node.flags != FS_NODE_FILE) {
        return 0;
    }
    handle = (fd_handle_t*)malloc(sizeof(fd_handle_t));
    if (!handle) return 0;
    memset(handle, 0, sizeof(*handle));
    handle->kind = node.flags == FS_NODE_DIR ? FD_KIND_DIR : FD_KIND_FILE;
    handle->refs = 1U;
    handle->access = access;
    handle->flags = open_flags;
    handle->fd_flags = (open_flags & FD_OPEN_CLOEXEC) != 0U ? NARCOS_FD_CLOEXEC : 0U;
    handle->offset = (open_flags & FD_OPEN_APPEND) != 0U ? node.size : 0U;
    handle->u.file.node_idx = node_idx;
    return handle;
}

static fd_handle_t* fd_create_pipe_handle(pipe_t* pipe, uint8_t is_writer) {
    fd_handle_t* handle;

    if (!pipe) return 0;
    handle = (fd_handle_t*)malloc(sizeof(fd_handle_t));
    if (!handle) return 0;
    memset(handle, 0, sizeof(*handle));
    handle->kind = FD_KIND_PIPE;
    handle->refs = 1U;
    handle->access = is_writer ? FD_ACCESS_WRITE : FD_ACCESS_READ;
    handle->u.pipe_end.pipe = pipe;
    handle->u.pipe_end.is_writer = is_writer;
    if (is_writer) pipe_acquire_writer(pipe);
    else pipe_acquire_reader(pipe);
    return handle;
}

static void fd_handle_acquire(fd_handle_t* handle) {
    if (!handle) return;
    handle->refs++;
}

static void fd_handle_release(fd_handle_t* handle) {
    if (!handle || handle->refs == 0U) return;
    handle->refs--;
    if (handle->refs != 0U) return;

    if (handle->kind == FD_KIND_PIPE && handle->u.pipe_end.pipe) {
        if (handle->u.pipe_end.is_writer) pipe_release_writer(handle->u.pipe_end.pipe);
        else pipe_release_reader(handle->u.pipe_end.pipe);
    }
    free(handle);
}

static int fd_slot_valid(int fd) {
    return fd >= 0 && fd < PROCESS_MAX_FDS;
}

static int fd_find_free_slot(process_t* proc, int start_fd) {
    int begin = start_fd < 0 ? 0 : start_fd;

    if (!proc) return -1;
    for (int fd = begin; fd < PROCESS_MAX_FDS; fd++) {
        if (!proc->fd_table[fd]) return fd;
    }
    return -1;
}

static int fd_install_handle(process_t* proc, fd_handle_t* handle, int target_fd) {
    int fd;

    if (!proc || !handle) return -1;
    fd = target_fd >= 0 ? target_fd : fd_find_free_slot(proc, 0);
    if (!fd_slot_valid(fd)) return -1;
    if (proc->fd_table[fd]) fd_close(proc, fd);
    proc->fd_table[fd] = handle;
    return fd;
}

static void fd_fill_stat_from_node(int node_idx, const disk_fs_node_t* node, narcos_stat_t* out_stat) {
    uint32_t permissions;

    if (!node || !out_stat) return;
    memset(out_stat, 0, sizeof(*out_stat));
    permissions = NARCOS_S_IRUSR | NARCOS_S_IRGRP | NARCOS_S_IROTH;
    if (node->flags == FS_NODE_FILE) permissions |= NARCOS_S_IWUSR | NARCOS_S_IWGRP | NARCOS_S_IWOTH;
    if (node->flags == FS_NODE_DIR) permissions |= NARCOS_S_IXUSR | NARCOS_S_IXGRP | NARCOS_S_IXOTH;

    out_stat->size = sizeof(*out_stat);
    out_stat->mode = (node->flags == FS_NODE_DIR ? NARCOS_S_IFDIR : NARCOS_S_IFREG) | permissions;
    out_stat->nlink = 1U;
    out_stat->ino = (uint64_t)(uint32_t)(node_idx + 2);
    out_stat->file_size = node->flags == FS_NODE_FILE ? node->size : 0U;
    out_stat->blksize = 512U;
    out_stat->blocks = (out_stat->file_size + 511ULL) / 512ULL;
}

static int fd_should_abort_wait(process_t* proc) {
    if (!proc) return 1;
    return proc->killed != 0U || (proc->flags & PROCESS_FLAG_USER_EXIT_PENDING) != 0U;
}

int fd_init_process(process_t* proc, process_t* parent) {
    fd_handle_t* stdin_handle;
    fd_handle_t* stdout_handle;
    fd_handle_t* stderr_handle;

    if (!proc) return -1;
    memset(proc->fd_table, 0, sizeof(proc->fd_table));

    if (parent) {
        for (int fd = 0; fd < PROCESS_MAX_FDS; fd++) {
            proc->fd_table[fd] = parent->fd_table[fd];
            if (proc->fd_table[fd]) fd_handle_acquire(proc->fd_table[fd]);
        }
        return 0;
    }

    stdin_handle = fd_create_console_handle(FD_ACCESS_READ);
    stdout_handle = fd_create_console_handle(FD_ACCESS_WRITE);
    stderr_handle = fd_create_console_handle(FD_ACCESS_WRITE);
    if (!stdin_handle || !stdout_handle || !stderr_handle) {
        fd_handle_release(stdin_handle);
        fd_handle_release(stdout_handle);
        fd_handle_release(stderr_handle);
        return -1;
    }

    proc->fd_table[0] = stdin_handle;
    proc->fd_table[1] = stdout_handle;
    proc->fd_table[2] = stderr_handle;
    return 0;
}

void fd_cleanup_process(process_t* proc) {
    if (!proc) return;
    for (int fd = 0; fd < PROCESS_MAX_FDS; fd++) {
        if (!proc->fd_table[fd]) continue;
        fd_handle_release(proc->fd_table[fd]);
        proc->fd_table[fd] = 0;
    }
}

void fd_close_from(process_t* proc, int first_fd) {
    if (!proc) return;
    if (first_fd < 0) first_fd = 0;
    for (int fd = first_fd; fd < PROCESS_MAX_FDS; fd++) {
        if (!proc->fd_table[fd]) continue;
        (void)fd_close(proc, fd);
    }
}

int fd_read(process_t* proc, int fd, void* buffer, uint32_t len) {
    fd_handle_t* handle;

    if (!proc || !fd_slot_valid(fd) || (!buffer && len != 0U)) return -1;
    if (len == 0U) return 0;
    handle = proc->fd_table[fd];
    if (!handle || (handle->access & FD_ACCESS_READ) == 0U) return -1;

    if (handle->kind == FD_KIND_CONSOLE) {
        int status;

        for (;;) {
            status = console_input_read((char*)buffer, len);
            if (status != 0) return status;
            if (fd_should_abort_wait(proc)) return -1;
            process_yield();
        }
    }

    if (handle->kind == FD_KIND_FILE) {
        int status = fs_read_file_raw_by_idx(handle->u.file.node_idx, buffer, handle->offset, len);
        if (status > 0) handle->offset += (uint32_t)status;
        return status;
    }

    if (handle->kind == FD_KIND_PIPE) {
        pipe_t* pipe = handle->u.pipe_end.pipe;
        int total = 0;

        if (!pipe || handle->u.pipe_end.is_writer != 0U) return -1;
        for (;;) {
            int chunk = pipe_read_some(pipe, (uint8_t*)buffer + total, len - (uint32_t)total);

            if (chunk < 0) return -1;
            total += chunk;
            if ((uint32_t)total == len) return total;
            if (total != 0) return total;
            if (pipe_writer_count(pipe) == 0U) return 0;
            if (fd_should_abort_wait(proc)) return -1;
            process_yield();
        }
    }

    return -1;
}

int fd_write(process_t* proc, int fd, const void* buffer, uint32_t len) {
    fd_handle_t* handle;

    if (!proc || !fd_slot_valid(fd) || (!buffer && len != 0U)) return -1;
    if (len == 0U) return 0;
    handle = proc->fd_table[fd];
    if (!handle || (handle->access & FD_ACCESS_WRITE) == 0U) return -1;
    if (handle->kind == FD_KIND_DIR) return -1;

    if (handle->kind == FD_KIND_CONSOLE) {
        vga_write((const char*)buffer, len);
        for (uint32_t i = 0; i < len; i++) {
            serial_write_char(((const char*)buffer)[i]);
        }
        return (int)len;
    }

    if (handle->kind == FD_KIND_FILE) {
        disk_fs_node_t node;

        if ((handle->flags & FD_OPEN_APPEND) != 0U &&
            fs_get_node_info(handle->u.file.node_idx, &node) == 0 && node.flags == FS_NODE_FILE) {
            handle->offset = node.size;
        }

        {
            int status = fs_write_file_raw_at_by_idx(handle->u.file.node_idx, buffer, handle->offset, len);
            if (status > 0) handle->offset += (uint32_t)status;
            return status;
        }
    }

    if (handle->kind == FD_KIND_PIPE) {
        pipe_t* pipe = handle->u.pipe_end.pipe;
        int total = 0;

        if (!pipe || handle->u.pipe_end.is_writer == 0U) return -1;
        if (pipe_reader_count(pipe) == 0U) return -1;

        for (;;) {
            int chunk = pipe_write_some(pipe, (const uint8_t*)buffer + total, len - (uint32_t)total);

            if (chunk < 0) return -1;
            total += chunk;
            if ((uint32_t)total == len) return total;
            if (total != 0 && pipe_reader_count(pipe) == 0U) return total;
            if (pipe_reader_count(pipe) == 0U) return -1;
            if (fd_should_abort_wait(proc)) return -1;
            process_yield();
        }
    }

    return -1;
}

int fd_is_console_write(process_t* proc, int fd) {
    fd_handle_t* handle;

    if (!proc || !fd_slot_valid(fd)) return 0;
    handle = proc->fd_table[fd];
    return handle && handle->kind == FD_KIND_CONSOLE &&
           (handle->access & FD_ACCESS_WRITE) != 0U;
}

int fd_close(process_t* proc, int fd) {
    fd_handle_t* handle;

    if (!proc || !fd_slot_valid(fd)) return -1;
    handle = proc->fd_table[fd];
    if (!handle) return -1;
    proc->fd_table[fd] = 0;
    fd_handle_release(handle);
    return 0;
}

int fd_dup(process_t* proc, int oldfd, int min_fd) {
    fd_handle_t* handle;
    int newfd;

    if (!proc || !fd_slot_valid(oldfd)) return -1;
    handle = proc->fd_table[oldfd];
    if (!handle) return -1;
    newfd = fd_find_free_slot(proc, min_fd);
    if (!fd_slot_valid(newfd)) return -1;
    fd_handle_acquire(handle);
    proc->fd_table[newfd] = handle;
    return newfd;
}

int fd_dup2(process_t* proc, int oldfd, int newfd) {
    fd_handle_t* handle;

    if (!proc || !fd_slot_valid(oldfd) || !fd_slot_valid(newfd)) return -1;
    handle = proc->fd_table[oldfd];
    if (!handle) return -1;
    if (oldfd == newfd) return newfd;

    if (proc->fd_table[newfd]) (void)fd_close(proc, newfd);
    fd_handle_acquire(handle);
    proc->fd_table[newfd] = handle;
    return newfd;
}

int fd_pipe(process_t* proc, int out_fds[2]) {
    pipe_t* pipe;
    fd_handle_t* read_handle;
    fd_handle_t* write_handle;
    int read_fd;
    int write_fd;

    if (!proc || !out_fds) return -1;
    pipe = pipe_create();
    if (!pipe) return -1;

    read_handle = fd_create_pipe_handle(pipe, 0U);
    write_handle = fd_create_pipe_handle(pipe, 1U);
    if (!read_handle || !write_handle) {
        fd_handle_release(read_handle);
        fd_handle_release(write_handle);
        return -1;
    }

    read_fd = fd_find_free_slot(proc, 0);
    write_fd = read_fd >= 0 ? fd_find_free_slot(proc, read_fd + 1) : -1;
    if (read_fd < 0 || write_fd < 0) {
        fd_handle_release(read_handle);
        fd_handle_release(write_handle);
        return -1;
    }

    proc->fd_table[read_fd] = read_handle;
    proc->fd_table[write_fd] = write_handle;
    out_fds[0] = read_fd;
    out_fds[1] = write_fd;
    return 0;
}

int fd_open_file(process_t* proc, const char* path, uint32_t access, uint32_t open_flags, int target_fd) {
    int node_idx;
    fd_handle_t* handle;
    int installed_fd;

    if (!proc || !path || path[0] == '\0') return -1;
    node_idx = fs_find_node(path);
    if (node_idx < 0 && (open_flags & FD_OPEN_CREATE) != 0U &&
        (open_flags & FD_OPEN_DIRECTORY) == 0U) {
        if (fs_create_file(path) != 0) return -1;
        node_idx = fs_find_node(path);
    }
    if (node_idx < 0) return -1;
    if ((open_flags & FD_OPEN_TRUNC) != 0U && (access & FD_ACCESS_WRITE) != 0U) {
        if (fs_write_file_raw_by_idx(node_idx, 0, 0U) < 0) return -1;
    }

    handle = fd_create_file_handle(node_idx, access, open_flags);
    if (!handle) return -1;
    installed_fd = fd_install_handle(proc, handle, target_fd);
    if (installed_fd < 0) {
        fd_handle_release(handle);
        return -1;
    }
    return installed_fd;
}

int fd_get_node_idx(process_t* proc, int fd, int* out_node_idx) {
    fd_handle_t* handle;

    if (!proc || !fd_slot_valid(fd) || !out_node_idx) return -1;
    handle = proc->fd_table[fd];
    if (!handle) return -1;
    if (handle->kind != FD_KIND_FILE && handle->kind != FD_KIND_DIR) return -1;
    *out_node_idx = handle->u.file.node_idx;
    return 0;
}

typedef struct __attribute__((packed)) {
    uint64_t d_ino;
    int64_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[256];
} fd_linux_dirent64_t;

int fd_getdents64(process_t* proc, int fd, void* buffer, uint32_t len) {
    fd_handle_t* handle;
    disk_fs_node_t entries[MAX_FILES];
    uint32_t written = 0U;
    int count;

    if (!proc || !fd_slot_valid(fd) || (!buffer && len != 0U)) return -1;
    handle = proc->fd_table[fd];
    if (!handle || handle->kind != FD_KIND_DIR) return -1;
    if (len < sizeof(fd_linux_dirent64_t)) return 0;

    count = fs_list_dir_entries_at(handle->u.file.node_idx, entries, MAX_FILES);
    if (count < 0) return -1;
    while (handle->offset < (uint32_t)count && written + sizeof(fd_linux_dirent64_t) <= len) {
        disk_fs_node_t* node = &entries[handle->offset];
        fd_linux_dirent64_t dent;

        memset(&dent, 0, sizeof(dent));
        dent.d_ino = (uint64_t)(handle->offset + 2U);
        dent.d_off = (int64_t)(handle->offset + 1U);
        dent.d_reclen = (uint16_t)sizeof(dent);
        dent.d_type = node->flags == FS_NODE_DIR ? 4U : 8U;
        strncpy(dent.d_name, node->name, sizeof(dent.d_name) - 1U);
        memcpy((uint8_t*)buffer + written, &dent, sizeof(dent));
        written += (uint32_t)sizeof(dent);
        handle->offset++;
    }
    return (int)written;
}

int fd_lseek(process_t* proc, int fd, int32_t offset, int whence, uint32_t* out_offset) {
    fd_handle_t* handle;
    disk_fs_node_t node;
    int64_t base;
    int64_t next;

    if (!proc || !fd_slot_valid(fd) || !out_offset) return -1;
    handle = proc->fd_table[fd];
    if (!handle || (handle->kind != FD_KIND_FILE && handle->kind != FD_KIND_DIR)) return -1;
    if (handle->kind == FD_KIND_DIR) {
        if (whence == NARCOS_SEEK_SET && offset >= 0) handle->offset = (uint32_t)offset;
        else if (whence == NARCOS_SEEK_CUR && (int64_t)handle->offset + (int64_t)offset >= 0) {
            handle->offset = (uint32_t)((int64_t)handle->offset + (int64_t)offset);
        } else return -1;
        *out_offset = handle->offset;
        return 0;
    }
    if (fs_get_node_info(handle->u.file.node_idx, &node) != 0 || node.flags != FS_NODE_FILE) return -1;

    if (whence == NARCOS_SEEK_SET) base = 0;
    else if (whence == NARCOS_SEEK_CUR) base = handle->offset;
    else if (whence == NARCOS_SEEK_END) base = node.size;
    else return -1;

    next = base + (int64_t)offset;
    if (next < 0 || next > (int64_t)MAX_FILE_SIZE) return -1;
    handle->offset = (uint32_t)next;
    *out_offset = handle->offset;
    return 0;
}

int fd_stat(process_t* proc, int fd, narcos_stat_t* out_stat) {
    fd_handle_t* handle;
    disk_fs_node_t node;

    if (!proc || !fd_slot_valid(fd) || !out_stat) return -1;
    handle = proc->fd_table[fd];
    if (!handle) return -1;

    if (handle->kind == FD_KIND_CONSOLE || handle->kind == FD_KIND_PIPE) {
        memset(out_stat, 0, sizeof(*out_stat));
        out_stat->size = sizeof(*out_stat);
        out_stat->mode = NARCOS_S_IFREG | NARCOS_S_IRUSR | NARCOS_S_IWUSR |
                         NARCOS_S_IRGRP | NARCOS_S_IWGRP |
                         NARCOS_S_IROTH | NARCOS_S_IWOTH;
        out_stat->nlink = 1U;
        out_stat->blksize = 512U;
        return 0;
    }

    if (handle->kind != FD_KIND_FILE && handle->kind != FD_KIND_DIR) return -1;
    if (fs_get_node_info(handle->u.file.node_idx, &node) != 0) return -1;
    fd_fill_stat_from_node(handle->u.file.node_idx, &node, out_stat);
    return 0;
}

int fd_fcntl(process_t* proc, int fd, int cmd, uintptr_t arg) {
    fd_handle_t* handle;

    if (!proc || !fd_slot_valid(fd)) return -1;
    handle = proc->fd_table[fd];
    if (!handle) return -1;

    switch (cmd) {
        case NARCOS_F_DUPFD:
            return fd_dup(proc, fd, (int)arg);
        case NARCOS_F_GETFD:
            return (int)handle->fd_flags;
        case NARCOS_F_SETFD:
            handle->fd_flags = (uint32_t)arg & NARCOS_FD_CLOEXEC;
            return 0;
        case NARCOS_F_GETFL:
            return (int)handle->flags;
        case NARCOS_F_SETFL:
            handle->flags = (handle->flags & ~(FD_OPEN_APPEND | FD_OPEN_NONBLOCK)) |
                            (((uint32_t)arg & NARCOS_O_APPEND) ? FD_OPEN_APPEND : 0U) |
                            (((uint32_t)arg & NARCOS_O_NONBLOCK) ? FD_OPEN_NONBLOCK : 0U);
            return 0;
        default:
            return -38;
    }
}

typedef struct __attribute__((packed)) {
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
} fd_winsize_t;

int fd_ioctl(process_t* proc, int fd, uint32_t request, void* user_arg) {
    fd_handle_t* handle;
    fd_winsize_t wsz;

    if (!proc || !fd_slot_valid(fd)) return -1;
    handle = proc->fd_table[fd];
    if (!handle) return -1;
    if (request != NARCOS_TIOCGWINSZ) return -38;
    if (handle->kind != FD_KIND_CONSOLE) return -25;
    memset(&wsz, 0, sizeof(wsz));
    wsz.ws_row = 25U;
    wsz.ws_col = 80U;
    if (user_arg) memcpy(user_arg, &wsz, sizeof(wsz));
    return 0;
}
