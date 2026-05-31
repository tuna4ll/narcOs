#ifndef FD_H
#define FD_H

#include <stdint.h>
#include "pipe.h"
#include "process.h"
#include "syscall.h"

#define FD_ACCESS_READ  0x01U
#define FD_ACCESS_WRITE 0x02U

#define FD_OPEN_CREATE 0x01U
#define FD_OPEN_TRUNC  0x02U
#define FD_OPEN_APPEND 0x04U
#define FD_OPEN_DIRECTORY 0x08U
#define FD_OPEN_NONBLOCK 0x10U
#define FD_OPEN_CLOEXEC 0x20U

typedef enum {
    FD_KIND_NONE = 0,
    FD_KIND_CONSOLE,
    FD_KIND_FILE,
    FD_KIND_PIPE,
    FD_KIND_DIR
} fd_kind_t;

struct fd_handle {
    fd_kind_t kind;
    uint32_t refs;
    uint32_t access;
    uint32_t flags;
    uint32_t fd_flags;
    uint32_t offset;
    union {
        struct {
            int node_idx;
        } file;
        struct {
            pipe_t* pipe;
            uint8_t is_writer;
        } pipe_end;
    } u;
};

int fd_init_process(process_t* proc, process_t* parent);
void fd_cleanup_process(process_t* proc);
void fd_close_from(process_t* proc, int first_fd);
int fd_read(process_t* proc, int fd, void* buffer, uint32_t len);
int fd_write(process_t* proc, int fd, const void* buffer, uint32_t len);
int fd_is_console_write(process_t* proc, int fd);
int fd_close(process_t* proc, int fd);
int fd_dup(process_t* proc, int oldfd, int min_fd);
int fd_dup2(process_t* proc, int oldfd, int newfd);
int fd_pipe(process_t* proc, int out_fds[2]);
int fd_open_file(process_t* proc, const char* path, uint32_t access, uint32_t open_flags, int target_fd);
int fd_get_node_idx(process_t* proc, int fd, int* out_node_idx);
int fd_getdents64(process_t* proc, int fd, void* buffer, uint32_t len);
int fd_lseek(process_t* proc, int fd, int32_t offset, int whence, uint32_t* out_offset);
int fd_stat(process_t* proc, int fd, narcos_stat_t* out_stat);
int fd_fcntl(process_t* proc, int fd, int cmd, uintptr_t arg);
int fd_ioctl(process_t* proc, int fd, uint32_t request, void* user_arg);

#endif
