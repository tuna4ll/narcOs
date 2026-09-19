#include <kernel/string.h>
#include <kernel/vfs.h>

#define TAR_BLOCK 512
#define S_IFREG 0100000
#define S_IFDIR 0040000

struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char type;
    char link[100];
    char magic[6];
    char version[2];
    char owner[32];
    char group[32];
    char major[8];
    char minor[8];
    char prefix[155];
    char pad[12];
};

static const uint8_t *tar_data;
static uint64_t tar_size;

static size_t str_len(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static int str_eq(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static uint64_t octal(const char *s, size_t len) {
    uint64_t value = 0;
    for (size_t i = 0; i < len && s[i]; i++) {
        if (s[i] < '0' || s[i] > '7') continue;
        value = (value << 3) + (uint64_t)(s[i] - '0');
    }
    return value;
}

static int zero_block(const uint8_t *p) {
    for (size_t i = 0; i < TAR_BLOCK; i++) if (p[i]) return 0;
    return 1;
}

static uint64_t next_offset(uint64_t offset, const struct tar_header *header) {
    uint64_t size = octal(header->size, sizeof(header->size));
    return offset + TAR_BLOCK + ((size + TAR_BLOCK - 1) & ~(TAR_BLOCK - 1));
}

static int make_path(const struct tar_header *header, char *out) {
    char raw[VFS_PATH_MAX];
    size_t n = 0;
    if (header->prefix[0]) {
        while (n < sizeof(header->prefix) && header->prefix[n] && n + 1 < sizeof(raw)) {
            raw[n] = header->prefix[n];
            n++;
        }
        if (n + 1 >= sizeof(raw)) return -1;
        raw[n++] = '/';
    }
    for (size_t i = 0; i < sizeof(header->name) && header->name[i]; i++) {
        if (n + 1 >= sizeof(raw)) return -1;
        raw[n++] = header->name[i];
    }
    raw[n] = 0;

    const char *src = raw;
    while (src[0] == '.' && src[1] == '/') src += 2;
    while (*src == '/') src++;
    out[0] = '/';
    n = 1;
    while (*src && n + 1 < VFS_PATH_MAX) out[n++] = *src++;
    if (*src) return -1;
    while (n > 1 && out[n - 1] == '/') n--;
    out[n] = 0;
    return 0;
}

static int normalize(const char *path, char *out) {
    if (!path || *path != '/') return -1;
    size_t n = 0;
    while (*path && n + 1 < VFS_PATH_MAX) {
        if (*path == '/' && n && out[n - 1] == '/') {
            path++;
            continue;
        }
        out[n++] = *path++;
    }
    if (*path) return -1;
    while (n > 1 && out[n - 1] == '/') n--;
    out[n] = 0;
    return 0;
}

static int header_at(uint64_t offset, const struct tar_header **header) {
    if (offset > tar_size || tar_size - offset < TAR_BLOCK) return 0;
    const struct tar_header *h = (const struct tar_header *)(tar_data + offset);
    if (zero_block((const uint8_t *)h)) return 0;
    uint64_t next = next_offset(offset, h);
    if (next < offset || next > tar_size) return 0;
    *header = h;
    return 1;
}

int vfs_init(const void *archive, uint64_t size) {
    if (!archive || size < TAR_BLOCK * 2) return -1;
    tar_data = archive;
    tar_size = size;
    const struct tar_header *header;
    for (uint64_t offset = 0; header_at(offset, &header); offset = next_offset(offset, header)) {
        if (header->magic[0] != 'u' || header->magic[1] != 's' || header->magic[2] != 't' ||
            header->magic[3] != 'a' || header->magic[4] != 'r') return -1;
    }
    return 0;
}

int vfs_open(const char *path, struct file *file) {
    char wanted[VFS_PATH_MAX];
    if (!file || normalize(path, wanted) != 0) return -1;
    memset(file, 0, sizeof(*file));
    if (str_eq(wanted, "/")) {
        file->node.type = VFS_DIR;
        file->node.mode = S_IFDIR | 0555;
        file->node.ino = 1;
        file->node.path[0] = '/';
        return 0;
    }

    const struct tar_header *header;
    for (uint64_t offset = 0; header_at(offset, &header); offset = next_offset(offset, header)) {
        char found[VFS_PATH_MAX];
        if (make_path(header, found) != 0 || !str_eq(found, wanted)) continue;
        size_t len = str_len(found);
        memcpy(file->node.path, found, len + 1);
        file->node.size = octal(header->size, sizeof(header->size));
        file->node.ino = offset / TAR_BLOCK + 2;
        file->node.type = header->type == '5' ? VFS_DIR : VFS_REG;
        file->node.mode = (file->node.type == VFS_DIR ? S_IFDIR | 0555 : S_IFREG | 0444);
        file->node.data = (const uint8_t *)header + TAR_BLOCK;
        return 0;
    }
    return -1;
}

long vfs_read(struct file *file, void *buf, size_t len) {
    if (!file || file->node.type != VFS_REG) return -1;
    if (file->offset >= file->node.size) return 0;
    uint64_t left = file->node.size - file->offset;
    if ((uint64_t)len > left) len = (size_t)left;
    memcpy(buf, file->node.data + file->offset, len);
    file->offset += len;
    return (long)len;
}

long vfs_seek(struct file *file, int64_t offset, int whence) {
    if (!file || file->node.type != VFS_REG) return -1;
    int64_t base = whence == 0 ? 0 : whence == 1 ? (int64_t)file->offset :
                   whence == 2 ? (int64_t)file->node.size : -1;
    if (base < 0 || offset < -base) return -1;
    uint64_t next = (uint64_t)(base + offset);
    if (next > file->node.size) return -1;
    file->offset = next;
    return (long)next;
}

int vfs_readdir(struct file *file, struct vfs_dirent *entry) {
    if (!file || !entry || file->node.type != VFS_DIR) return -1;
    const struct tar_header *header;
    uint64_t offset = file->offset;
    while (header_at(offset, &header)) {
        uint64_t current = offset;
        offset = next_offset(offset, header);
        file->offset = offset;
        char path[VFS_PATH_MAX];
        if (make_path(header, path) != 0) continue;

        const char *name;
        if (str_eq(file->node.path, "/")) {
            name = path + 1;
        } else {
            size_t base = str_len(file->node.path);
            size_t i = 0;
            while (i < base && path[i] == file->node.path[i]) i++;
            if (i != base || path[i] != '/') continue;
            name = path + i + 1;
        }
        if (!*name) continue;
        size_t len = 0;
        while (name[len] && name[len] != '/') len++;
        if (name[len] || len >= VFS_NAME_MAX) continue;
        entry->ino = current / TAR_BLOCK + 2;
        entry->type = header->type == '5' ? VFS_DIR : VFS_REG;
        memcpy(entry->name, name, len);
        entry->name[len] = 0;
        return 1;
    }
    return 0;
}
