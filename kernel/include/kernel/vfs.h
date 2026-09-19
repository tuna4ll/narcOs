#pragma once
#include <stddef.h>
#include <stdint.h>

#define VFS_PATH_MAX 128
#define VFS_NAME_MAX 64
#define VFS_REG 1
#define VFS_DIR 2

struct vnode {
    const uint8_t *data;
    uint64_t size;
    uint64_t ino;
    uint32_t mode;
    uint8_t type;
    char path[VFS_PATH_MAX];
};

struct file {
    struct vnode node;
    uint64_t offset;
};

struct vfs_dirent {
    uint64_t ino;
    uint8_t type;
    char name[VFS_NAME_MAX];
};

int vfs_init(const void *archive, uint64_t size);
int vfs_open(const char *path, struct file *file);
long vfs_read(struct file *file, void *buf, size_t len);
long vfs_seek(struct file *file, int64_t offset, int whence);
int vfs_readdir(struct file *file, struct vfs_dirent *entry);
