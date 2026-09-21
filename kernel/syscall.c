#include <kernel/console.h>
#include <kernel/mm.h>
#include <kernel/syscall.h>
#include <kernel/task.h>
#include <kernel/string.h>
#include <kernel/vfs.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__x86_64__)
#define SYS_READ               0
#define SYS_WRITE              1
#define SYS_OPEN               2
#define SYS_CLOSE              3
#define SYS_STAT               4
#define SYS_FSTAT              5
#define SYS_LSEEK              8
#define SYS_MMAP               9
#define SYS_MPROTECT          10
#define SYS_MUNMAP            11
#define SYS_BRK               12
#define SYS_IOCTL             16
#define SYS_WRITEV            20
#define SYS_SCHED_YIELD       24
#define SYS_MADVISE           28
#define SYS_GETPID            39
#define SYS_FCNTL             72
#define SYS_EXIT              60
#define SYS_ARCH_PRCTL       158
#define SYS_SET_TID_ADDRESS  218
#define SYS_EXIT_GROUP       231
#define SYS_OPENAT           257
#define SYS_NEWFSTATAT       262
#define SYS_GETDENTS64       217
#else
#define SYS_FCNTL             25
#define SYS_IOCTL             29
#define SYS_OPENAT            56
#define SYS_CLOSE             57
#define SYS_GETDENTS64        61
#define SYS_LSEEK             62
#define SYS_READ              63
#define SYS_WRITE             64
#define SYS_WRITEV            66
#define SYS_NEWFSTATAT        79
#define SYS_FSTAT             80
#define SYS_EXIT              93
#define SYS_EXIT_GROUP        94
#define SYS_SET_TID_ADDRESS   96
#define SYS_SCHED_YIELD      124
#define SYS_GETPID           172
#define SYS_BRK              214
#define SYS_MUNMAP           215
#define SYS_MMAP             222
#define SYS_MPROTECT         226
#define SYS_MADVISE          233
#endif

#define EBADF   9
#define ENOENT  2
#define EIO     5
#define EFAULT 14
#define ENOMEM 12
#define EINVAL 22
#define EMFILE 24
#define ENOTTY 25
#define EISDIR 21
#define ENOTDIR 20
#define EROFS 30
#define ENOSYS 38

#define PROT_WRITE           0x2
#define MAP_FIXED            0x10
#define MAP_ANON             0x20
#define MAP_FIXED_NOREPLACE  0x100000
#define ARCH_SET_FS          0x1002
#define ARCH_GET_FS          0x1003
#define TIOCGWINSZ           0x5413
#define O_ACCMODE            0x3
#define O_DIRECTORY          0x10000
#define AT_FDCWD            -100
#define AT_EMPTY_PATH        0x1000
#define F_GETFD              1
#define F_SETFD              2
#define USER_MMAP_BASE 0x0000000100000000ULL
#define USER_MMAP_END  0x0000000140000000ULL

struct iovec64 {
    uint64_t base;
    uint64_t len;
};

struct winsize64 {
    uint16_t rows;
    uint16_t cols;
    uint16_t xpixel;
    uint16_t ypixel;
};

#if defined(__x86_64__)
struct kernel_stat {
    uint64_t dev;
    uint64_t ino;
    uint64_t nlink;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t pad;
    uint64_t rdev;
    int64_t size;
    int64_t blksize;
    int64_t blocks;
    int64_t times[6];
    int64_t unused[3];
};
#else
struct kernel_stat {
    uint64_t dev;
    uint64_t ino;
    uint32_t mode;
    uint32_t nlink;
    uint32_t uid;
    uint32_t gid;
    uint64_t rdev;
    uint64_t pad1;
    int64_t size;
    int32_t blksize;
    int32_t pad2;
    int64_t blocks;
    int64_t times[6];
    uint32_t unused[2];
};
#endif

static uint64_t align_up(uint64_t value) {
    if (value > UINT64_MAX - (PAGE_SIZE - 1)) return 0;
    return (value + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static int copy_from_user(void *dst, uint64_t src, size_t len) {
    uint8_t *out = dst;
    struct address_space *space = vmm_space_current();
    if (!vmm_user_range_ok(space, src, len, 0)) return -1;

    while (len) {
        uint64_t phys = vmm_user_phys(space, src);
        if (!phys) return -1;

        size_t chunk = PAGE_SIZE - (size_t)(src & (PAGE_SIZE - 1));
        if (chunk > len) chunk = len;

        const uint8_t *in = phys_to_virt(phys);
        for (size_t i = 0; i < chunk; i++) out[i] = in[i];

        src += chunk;
        out += chunk;
        len -= chunk;
    }
    return 0;
}

static int copy_to_user(uint64_t dst, const void *src, size_t len) {
    const uint8_t *in = src;
    struct address_space *space = vmm_space_current();
    if (!vmm_user_range_ok(space, dst, len, 1)) return -1;

    while (len) {
        uint64_t phys = vmm_user_phys(space, dst);
        if (!phys) return -1;

        size_t chunk = PAGE_SIZE - (size_t)(dst & (PAGE_SIZE - 1));
        if (chunk > len) chunk = len;

        uint8_t *out = phys_to_virt(phys);
        for (size_t i = 0; i < chunk; i++) out[i] = in[i];

        dst += chunk;
        in += chunk;
        len -= chunk;
    }
    return 0;
}

static int copy_string(char *dst, uint64_t src, size_t cap) {
    if (!cap) return -1;
    for (size_t i = 0; i < cap; i++) {
        if (copy_from_user(&dst[i], src + i, 1) != 0) return -1;
        if (!dst[i]) return 0;
    }
    return -1;
}

static long sys_read(uint64_t fd, uint64_t buf, uint64_t len) {
    if (fd == 0) return 0;
    struct file *file = task_fd_get((int)fd);
    if (!file) return -EBADF;
    if (file->node.type == VFS_DIR) return -EISDIR;
    if (!vmm_user_range_ok(vmm_space_current(), buf, len, 1)) return -EFAULT;

    uint8_t chunk[512];
    uint64_t done = 0;
    while (done < len) {
        size_t want = len - done > sizeof(chunk) ? sizeof(chunk) : (size_t)(len - done);
        long got = vfs_read(file, chunk, want);
        if (got < 0) return done ? (long)done : -EIO;
        if (!got) break;
        if (copy_to_user(buf + done, chunk, (size_t)got) != 0) return -EFAULT;
        done += (uint64_t)got;
    }
    return (long)done;
}

static long sys_write(uint64_t fd, uint64_t buf, uint64_t len) {
    if (fd != 1 && fd != 2) return -EBADF;
    struct address_space *space = vmm_space_current();
    if (!vmm_user_range_ok(space, buf, len, 0)) return -EFAULT;

    for (uint64_t left = len, ptr = buf; left;) {
        uint64_t phys = vmm_user_phys(space, ptr);
        if (!phys) return -EFAULT;

        size_t chunk = PAGE_SIZE - (size_t)(ptr & (PAGE_SIZE - 1));
        if ((uint64_t)chunk > left) chunk = (size_t)left;
        console_write(phys_to_virt(phys), chunk);

        ptr += chunk;
        left -= chunk;
    }
    return (long)len;
}

static long sys_open_file(int64_t dirfd, uint64_t path_addr, uint64_t flags) {
    char path[VFS_PATH_MAX];
    if (copy_string(path, path_addr, sizeof(path)) != 0) return -EFAULT;
    if (path[0] != '/') return dirfd == AT_FDCWD ? -ENOENT : -EBADF;
    if (flags & O_ACCMODE) return -EROFS;
    int fd = task_fd_open(path);
    if (fd == -1) return -ENOENT;
    if (fd == -2) return -EMFILE;
    struct file *file = task_fd_get(fd);
    if ((flags & O_DIRECTORY) && file->node.type != VFS_DIR) {
        task_fd_close(fd);
        return -ENOTDIR;
    }
    return fd;
}

static void make_stat(const struct vnode *node, struct kernel_stat *st) {
    memset(st, 0, sizeof(*st));
    st->dev = 1;
    st->ino = node->ino;
    st->nlink = 1;
    st->mode = node->mode;
    st->size = (int64_t)node->size;
    st->blksize = 512;
    st->blocks = (int64_t)((node->size + 511) / 512);
}

static long put_stat(const struct vnode *node, uint64_t addr) {
    struct kernel_stat st;
    make_stat(node, &st);
    return copy_to_user(addr, &st, sizeof(st)) == 0 ? 0 : -EFAULT;
}

static long sys_fstat(uint64_t fd, uint64_t addr) {
    if (fd <= 2) {
        struct vnode node = { .ino = fd + 1, .mode = 0020000 | 0666 };
        return put_stat(&node, addr);
    }
    struct file *file = task_fd_get((int)fd);
    return file ? put_stat(&file->node, addr) : -EBADF;
}

#if defined(__x86_64__)
static long sys_stat(uint64_t path_addr, uint64_t addr) {
    char path[VFS_PATH_MAX];
    struct file file;
    if (copy_string(path, path_addr, sizeof(path)) != 0) return -EFAULT;
    if (vfs_open(path, &file) != 0) return -ENOENT;
    return put_stat(&file.node, addr);
}
#endif

static long sys_fstatat(int64_t dirfd, uint64_t path_addr, uint64_t addr, uint64_t flags) {
    char path[VFS_PATH_MAX];
    if (copy_string(path, path_addr, sizeof(path)) != 0) return -EFAULT;
    if (!path[0] && (flags & AT_EMPTY_PATH)) return sys_fstat((uint64_t)dirfd, addr);
    if (path[0] != '/') return dirfd == AT_FDCWD ? -ENOENT : -EBADF;
    struct file file;
    if (vfs_open(path, &file) != 0) return -ENOENT;
    return put_stat(&file.node, addr);
}

static long sys_close(uint64_t fd) {
    if (fd <= 2) return 0;
    return task_fd_close((int)fd) == 0 ? 0 : -EBADF;
}

static long sys_lseek(uint64_t fd, int64_t offset, uint64_t whence) {
    struct file *file = task_fd_get((int)fd);
    if (!file) return -EBADF;
    long result = vfs_seek(file, offset, (int)whence);
    return result < 0 ? -EINVAL : result;
}

static long sys_getdents(uint64_t fd, uint64_t addr, uint64_t count) {
    struct file *file = task_fd_get((int)fd);
    if (!file) return -EBADF;
    if (file->node.type != VFS_DIR) return -ENOTDIR;
    uint64_t written = 0;
    for (;;) {
        uint64_t saved = file->offset;
        struct vfs_dirent entry;
        int result = vfs_readdir(file, &entry);
        if (result < 0) return -ENOTDIR;
        if (!result) break;
        size_t name_len = 0;
        while (entry.name[name_len]) name_len++;
        uint16_t reclen = (uint16_t)((19 + name_len + 1 + 7) & ~7ULL);
        if (reclen > count - written) {
            file->offset = saved;
            if (!written) return -EINVAL;
            break;
        }
        uint8_t record[96];
        memset(record, 0, sizeof(record));
        int64_t next = (int64_t)file->offset;
        uint8_t type = entry.type == VFS_DIR ? 4 : 8;
        memcpy(record, &entry.ino, sizeof(entry.ino));
        memcpy(record + 8, &next, sizeof(next));
        memcpy(record + 16, &reclen, sizeof(reclen));
        record[18] = type;
        memcpy(record + 19, entry.name, name_len + 1);
        if (copy_to_user(addr + written, record, reclen) != 0) return -EFAULT;
        written += reclen;
    }
    return (long)written;
}

static long sys_fcntl(uint64_t fd, uint64_t cmd) {
    if (fd > 2 && !task_fd_get((int)fd)) return -EBADF;
    if (cmd == F_GETFD || cmd == F_SETFD) return 0;
    return -EINVAL;
}

static long sys_writev(uint64_t fd, uint64_t addr, uint64_t count) {
    if (fd != 1 && fd != 2) return -EBADF;
    if (count > 1024) return -EINVAL;

    long total = 0;
    for (uint64_t i = 0; i < count; i++) {
        struct iovec64 iov;
        if (copy_from_user(&iov, addr + i * sizeof(iov), sizeof(iov)) != 0) return -EFAULT;
        long ret = sys_write(fd, iov.base, iov.len);
        if (ret < 0) return ret;
        total += ret;
    }
    return total;
}

static long sys_mmap(uint64_t addr, uint64_t len, uint64_t prot, uint64_t flags, uint64_t fd) {
    if (!len) return -EINVAL;
    if (!(flags & MAP_ANON) || (int64_t)fd != -1) return -ENOSYS;

    uint64_t size = align_up(len);
    if (!size) return -ENOMEM;

    uint64_t base = (flags & (MAP_FIXED | MAP_FIXED_NOREPLACE)) ? addr : align_up(task_mmap_next());
    if ((base & (PAGE_SIZE - 1)) || base < USER_MMAP_BASE || size > USER_MMAP_END - base) return -ENOMEM;

    struct address_space *space = vmm_space_current();
    uint64_t mapped = 0;
    for (uint64_t off = 0; off < size; off += PAGE_SIZE) {
        uint64_t va = base + off;
        if (vmm_user_phys(space, va)) {
            if (flags & MAP_FIXED_NOREPLACE) goto fail;
            if (!(flags & MAP_FIXED)) goto fail;
            vmm_unmap_user(space, va);
        }

        uint64_t phys = pmm_alloc_page();
        if (!phys) goto fail;
        if (vmm_map_user(space, va, phys, (prot & PROT_WRITE) ? VMM_WRITE : 0) != 0) {
            pmm_free_page(phys);
            goto fail;
        }
        mapped += PAGE_SIZE;
    }

    if (!(flags & (MAP_FIXED | MAP_FIXED_NOREPLACE))) task_set_mmap_next(base + size);
    return (long)base;

fail:
    for (uint64_t off = 0; off < mapped; off += PAGE_SIZE) vmm_unmap_user(space, base + off);
    return -ENOMEM;
}

static long sys_mprotect(uint64_t addr, uint64_t len, uint64_t prot) {
    if ((addr & (PAGE_SIZE - 1)) || !len) return -EINVAL;
    uint64_t size = align_up(len);
    if (!size) return -EINVAL;

    for (uint64_t off = 0; off < size; off += PAGE_SIZE) {
        if (vmm_protect_user(vmm_space_current(), addr + off,
                             (prot & PROT_WRITE) ? VMM_WRITE : 0) != 0) return -ENOMEM;
    }
    return 0;
}

static long sys_munmap(uint64_t addr, uint64_t len) {
    if ((addr & (PAGE_SIZE - 1)) || !len) return -EINVAL;
    uint64_t size = align_up(len);
    if (!size) return -EINVAL;

    for (uint64_t off = 0; off < size; off += PAGE_SIZE)
        vmm_unmap_user(vmm_space_current(), addr + off);
    return 0;
}

static long sys_ioctl(uint64_t fd, uint64_t request, uint64_t arg) {
    if (fd != 1 && fd != 2) return -EBADF;
    if (request != TIOCGWINSZ) return -ENOTTY;

    struct winsize64 ws = {
        .rows = console_rows(),
        .cols = console_cols(),
    };
    return copy_to_user(arg, &ws, sizeof(ws)) == 0 ? 0 : -EFAULT;
}

#if defined(__x86_64__)
static long sys_arch_prctl(uint64_t code, uint64_t addr) {
    if (code == ARCH_SET_FS) {
        if (addr > 0x00007fffffffffffULL) return -EINVAL;
        task_set_fs_base(addr);
        return 0;
    }
    if (code == ARCH_GET_FS) {
        uint64_t fs_base = task_fs_base();
        return copy_to_user(addr, &fs_base, sizeof(fs_base)) == 0 ? 0 : -EFAULT;
    }
    return -EINVAL;
}
#endif

static long dispatch(uint64_t nr, uint64_t a1, uint64_t a2, uint64_t a3,
                     uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a6;

    switch (nr) {
    case SYS_READ:            return sys_read(a1, a2, a3);
    case SYS_WRITE:           return sys_write(a1, a2, a3);
#if defined(__x86_64__)
    case SYS_OPEN:            return sys_open_file(AT_FDCWD, a1, a2);
#endif
    case SYS_CLOSE:           return sys_close(a1);
#if defined(__x86_64__)
    case SYS_STAT:            return sys_stat(a1, a2);
#endif
    case SYS_FSTAT:           return sys_fstat(a1, a2);
    case SYS_LSEEK:           return sys_lseek(a1, (int64_t)a2, a3);
    case SYS_MMAP:            return sys_mmap(a1, a2, a3, a4, a5);
    case SYS_MPROTECT:        return sys_mprotect(a1, a2, a3);
    case SYS_MUNMAP:          return sys_munmap(a1, a2);
    case SYS_BRK:             return 0;
    case SYS_IOCTL:           return sys_ioctl(a1, a2, a3);
    case SYS_WRITEV:          return sys_writev(a1, a2, a3);
    case SYS_MADVISE:         return 0;
    case SYS_GETPID:          return task_pid();
    case SYS_FCNTL:           return sys_fcntl(a1, a2);
#if defined(__x86_64__)
    case SYS_ARCH_PRCTL:      return sys_arch_prctl(a1, a2);
#endif
    case SYS_SET_TID_ADDRESS: return task_pid();
    case SYS_GETDENTS64:      return sys_getdents(a1, a2, a3);
    case SYS_OPENAT:          return sys_open_file((int64_t)a1, a2, a3);
    case SYS_NEWFSTATAT:      return sys_fstatat((int64_t)a1, a2, a3, a4);
    default:
        return -ENOSYS;
    }
}

void syscall_dispatch(struct task_frame *frame) {
    uint64_t nr = arch_syscall_number(frame);
    if (nr == SYS_SCHED_YIELD) {
        arch_syscall_return(frame, 0);
        task_yield(frame);
        return;
    }
    if (nr == SYS_EXIT || nr == SYS_EXIT_GROUP) {
        task_exit(frame);
        return;
    }
    arch_syscall_return(frame, (uint64_t)dispatch(nr, arch_syscall_arg(frame, 0),
                        arch_syscall_arg(frame, 1), arch_syscall_arg(frame, 2),
                        arch_syscall_arg(frame, 3), arch_syscall_arg(frame, 4),
                        arch_syscall_arg(frame, 5)));
}
