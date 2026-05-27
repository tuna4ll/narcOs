#include "fs.h"
#include "string.h"
#include "storage.h"
#include "memory_alloc.h"
#include "serial.h"

extern void vga_print(const char* str);
extern void vga_print_color(const char* str, uint8_t color);
extern void vga_println(const char* str);
extern void vga_print_int(int num);
extern void vga_print_int_hex(uint32_t n, char* buf);

#define DIR_SECTOR 3072
#define DIR_SECTOR_COUNT 8
#define DATA_START_SECTOR 4096
#define DATA_END_SECTOR 6144
#define FS_ROOT_INDEX (-1)
#define FS_INVALID_INDEX (-2)
#define FS_NODE_FLAG_EXTERNAL_BLOB 0x58424C42U
#define NARCOS_BOOT_INFO_ADDR 0x7000U
#define NARCOS_BOOT_INFO_MAGIC 0x4243524EU
#define NARCOS_BOOT_INFO_VERSION_WITH_INITRD_ADDR 4U
#define NARCOS_BOOT_INFO_INITRD_ADDR_SIZE 104U
#define FS_BOOT_INITRD_MISS (-2)

disk_fs_node_t dir_cache[MAX_FILES];
uint8_t sector_buffer[512];
int current_dir_index = -1;
static int fs_ram_overlay_enabled = 0;
static int fs_boot_info_cached = 0;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t flags;
    uint8_t boot_drive;
    uint8_t profile;
    uint16_t reserved;
    uint32_t kernel_lba;
    uint32_t kernel_sectors;
    uint16_t vbe_mode;
    uint16_t target_width;
    uint16_t target_height;
    uint16_t e820_count;
    uint32_t framebuffer_addr;
    uint32_t framebuffer_size;
    uint32_t kernel_load_addr;
    uint32_t kernel_load_size;
    uint16_t fb_width;
    uint16_t fb_height;
    uint16_t fb_pitch;
    uint8_t fb_bpp;
    uint8_t fb_memory_model;
    uint8_t red_mask;
    uint8_t red_position;
    uint8_t green_mask;
    uint8_t green_position;
    uint8_t blue_mask;
    uint8_t blue_position;
    uint8_t rsv_mask;
    uint8_t rsv_position;
    uint32_t e820_map_addr;
    uint16_t e820_entry_size;
    uint16_t boot_manifest_version;
    uint32_t rsdp_addr;
    uint32_t kernel_entry;
    uint32_t kernel_crc32;
    uint32_t initrd_lba;
    uint32_t initrd_sectors;
    uint32_t initrd_size;
    uint32_t initrd_crc32;
    uint32_t initrd_addr;
} __attribute__((packed)) fs_boot_info_t;

typedef struct fs_ram_overlay_sector {
    uint32_t lba;
    struct fs_ram_overlay_sector* next;
    uint8_t data[512];
} fs_ram_overlay_sector_t;

static fs_ram_overlay_sector_t* fs_ram_overlay_head = 0;
static fs_boot_info_t fs_boot_info_cache;

static int fs_alloc_data_run(uint32_t sectors, int ignore_idx);
static void fs_zero_sectors(uint32_t lba, uint32_t count);
static int fs_find_node_internal(const char* path);

static const fs_boot_info_t* fs_boot_info_get(void) {
    const fs_boot_info_t* info = (const fs_boot_info_t*)(uintptr_t)NARCOS_BOOT_INFO_ADDR;

    if (fs_boot_info_cached) return &fs_boot_info_cache;
    if (info->magic != NARCOS_BOOT_INFO_MAGIC) return 0;
    if (info->version < NARCOS_BOOT_INFO_VERSION_WITH_INITRD_ADDR) return 0;
    if (info->size < NARCOS_BOOT_INFO_INITRD_ADDR_SIZE) return 0;
    if (info->initrd_addr == 0U || info->initrd_lba == 0U || info->initrd_sectors == 0U) return 0;

    fs_boot_info_cache = *info;
    fs_boot_info_cached = 1;
    serial_write("[fs] cached boot initrd lba=");
    serial_write_hex32(fs_boot_info_cache.initrd_lba);
    serial_write(" sectors=");
    serial_write_hex32(fs_boot_info_cache.initrd_sectors);
    serial_write(" addr=");
    serial_write_hex32(fs_boot_info_cache.initrd_addr);
    serial_write_char('\n');
    return &fs_boot_info_cache;
}

static int fs_read_boot_initrd_blob(uint32_t src_lba, size_t len, void* buffer,
                                    size_t offset, size_t max_len) {
    const fs_boot_info_t* info;
    uint8_t* bytes = (uint8_t*)buffer;
    size_t read_len;
    uint64_t initrd_capacity;
    uint64_t blob_base;
    uint64_t request_start;
    uint64_t request_end;

    if (!bytes && max_len != 0U) return -1;
    if (src_lba == 0U || offset >= len || max_len == 0U) return 0;

    info = fs_boot_info_get();
    if (!info || src_lba < info->initrd_lba) return FS_BOOT_INITRD_MISS;

    read_len = len - offset;
    if (read_len > max_len) read_len = max_len;

    initrd_capacity = (uint64_t)info->initrd_sectors * 512ULL;
    blob_base = (uint64_t)(src_lba - info->initrd_lba) * 512ULL;
    request_start = blob_base + (uint64_t)offset;
    request_end = request_start + (uint64_t)read_len;
    if (request_end < request_start || request_end > initrd_capacity) return FS_BOOT_INITRD_MISS;

    memcpy(bytes, (const uint8_t*)(uintptr_t)info->initrd_addr + request_start, read_len);
    return (int)read_len;
}

#if defined(NARCOS_DISK_DOOM_BIN) && defined(NARCOS_DISK_INITRD_LBA) && \
    defined(NARCOS_DISK_INITRD_SIZE) && defined(NARCOS_DISK_INITRD_ADDR)
static int fs_compiled_initrd_ready(void) {
    static int ready = -1;
    const uint8_t* bytes;

    if (ready >= 0) return ready;
    bytes = (const uint8_t*)(uintptr_t)NARCOS_DISK_INITRD_ADDR;
    ready = NARCOS_DISK_DOOM_BIN_SIZE >= 4U &&
            bytes[0] == 0x7FU && bytes[1] == 'E' &&
            bytes[2] == 'L' && bytes[3] == 'F';
    if (!ready) {
        serial_write_line("[fs] initrd memory fallback unavailable");
    }
    return ready;
}

static int fs_read_compiled_initrd_blob(uint32_t src_lba, size_t len, void* buffer,
                                        size_t offset, size_t max_len) {
    uint8_t* bytes = (uint8_t*)buffer;
    size_t read_len;
    uint64_t blob_base;
    uint64_t request_start;
    uint64_t request_end;

    if (!bytes && max_len != 0U) return -1;
    if (src_lba == 0U || offset >= len || max_len == 0U) return 0;
    if (!fs_compiled_initrd_ready()) return FS_BOOT_INITRD_MISS;
    if (src_lba < (uint32_t)NARCOS_DISK_INITRD_LBA) return FS_BOOT_INITRD_MISS;

    read_len = len - offset;
    if (read_len > max_len) read_len = max_len;

    blob_base = (uint64_t)(src_lba - (uint32_t)NARCOS_DISK_INITRD_LBA) * 512ULL;
    request_start = blob_base + (uint64_t)offset;
    request_end = request_start + (uint64_t)read_len;
    if (request_end < request_start || request_end > (uint64_t)NARCOS_DISK_INITRD_SIZE) {
        return FS_BOOT_INITRD_MISS;
    }

    memcpy(bytes, (const uint8_t*)(uintptr_t)NARCOS_DISK_INITRD_ADDR + request_start, read_len);
    return (int)read_len;
}
#else
static int fs_read_compiled_initrd_blob(uint32_t src_lba, size_t len, void* buffer,
                                        size_t offset, size_t max_len) {
    (void)src_lba;
    (void)len;
    (void)buffer;
    (void)offset;
    (void)max_len;
    return FS_BOOT_INITRD_MISS;
}
#endif

static int fs_ram_overlay_lba_allowed(uint32_t lba) {
    if (lba >= DIR_SECTOR && lba < DIR_SECTOR + DIR_SECTOR_COUNT) {
        return 1;
    }
    if (lba >= DATA_START_SECTOR && lba < DATA_END_SECTOR) {
        return 1;
    }
    return 0;
}

static fs_ram_overlay_sector_t* fs_find_ram_overlay_sector(uint32_t lba) {
    for (fs_ram_overlay_sector_t* sector = fs_ram_overlay_head; sector; sector = sector->next) {
        if (sector->lba == lba) return sector;
    }
    return 0;
}

static fs_ram_overlay_sector_t* fs_get_ram_overlay_sector(uint32_t lba) {
    fs_ram_overlay_sector_t* sector;

    if (!fs_ram_overlay_lba_allowed(lba)) return 0;
    sector = fs_find_ram_overlay_sector(lba);
    if (sector) return sector;

    sector = (fs_ram_overlay_sector_t*)malloc(sizeof(*sector));
    if (!sector) return 0;
    sector->lba = lba;
    sector->next = fs_ram_overlay_head;
    memset(sector->data, 0, sizeof(sector->data));
    fs_ram_overlay_head = sector;
    return sector;
}

static void fs_enable_ram_overlay(void) {
    if (fs_ram_overlay_enabled) return;
    fs_ram_overlay_enabled = 1;
    serial_write_line("[fs] storage is not writable; using volatile RAM filesystem overlay");
}

static int fs_storage_read_sector(uint32_t lba, uint8_t* buffer) {
    fs_ram_overlay_sector_t* sector;

    if (!buffer) return -1;
    sector = fs_find_ram_overlay_sector(lba);
    if (sector) {
        memcpy(buffer, sector->data, sizeof(sector->data));
        return 0;
    }
    return storage_read_sector(lba, buffer);
}

static int fs_storage_write_sector(uint32_t lba, const uint8_t* buffer) {
    fs_ram_overlay_sector_t* sector;

    if (!buffer) return -1;
    if (!fs_ram_overlay_enabled && storage_write_sector(lba, buffer) == 0) return 0;

    fs_enable_ram_overlay();
    sector = fs_get_ram_overlay_sector(lba);
    if (!sector) return -1;
    memcpy(sector->data, buffer, sizeof(sector->data));
    return 0;
}

#define FS_DISTINCT_USER_PROGRAMS(X) \
    X(hello) \
    X(ps) \
    X(cat) \
    X(echo) \
    X(kill) \
    X(proc_test) \
    X(pipe_test) \
    X(credits) \
    X(neofetch) \
    X(desktop) \
    X(explorer) \
    X(narcpad) \
    X(settings) \
    X(snake) \
    X(core_tools) \
    X(tls_tools)

#define FS_CORE_TOOL_ALIASES(X) \
    X(help, core_tools) \
    X(clear, core_tools) \
    X(ver, core_tools) \
    X(uptime, core_tools) \
    X(date, core_tools) \
    X(time, core_tools) \
    X(ls, core_tools) \
    X(pwd, core_tools) \
    X(http, core_tools) \
    X(ping, core_tools) \
    X(netdemo, core_tools) \
    X(dns, core_tools) \
    X(net, core_tools)

#define FS_TLS_TOOL_ALIASES(X) \
    X(https, tls_tools) \
    X(tls_test, tls_tools) \
    X(fetch, tls_tools)

#if UINTPTR_MAX > 0xFFFFFFFFU
#define FS_DECLARE_PACKAGED_BINARY(name) \
    extern const uint8_t _binary_obj_x86_64_user_bin_##name##_start[]; \
    extern const uint8_t _binary_obj_x86_64_user_bin_##name##_end[];
#else
#define FS_DECLARE_PACKAGED_BINARY(name) \
    extern const uint8_t _binary_obj_i386_user_bin_##name##_start[]; \
    extern const uint8_t _binary_obj_i386_user_bin_##name##_end[];
#endif

FS_DISTINCT_USER_PROGRAMS(FS_DECLARE_PACKAGED_BINARY)

typedef struct {
    const char* path;
    const uint8_t* start;
    const uint8_t* end;
} fs_packaged_binary_t;

static const fs_packaged_binary_t fs_packaged_binaries[] = {
#if UINTPTR_MAX > 0xFFFFFFFFU
#define FS_PACKAGED_BINARY_ENTRY(name) \
    { "/bin/" #name, _binary_obj_x86_64_user_bin_##name##_start, _binary_obj_x86_64_user_bin_##name##_end },
#define FS_PACKAGED_BINARY_ALIAS(alias, target) \
    { "/bin/" #alias, _binary_obj_x86_64_user_bin_##target##_start, _binary_obj_x86_64_user_bin_##target##_end },
#else
#define FS_PACKAGED_BINARY_ENTRY(name) \
    { "/bin/" #name, _binary_obj_i386_user_bin_##name##_start, _binary_obj_i386_user_bin_##name##_end },
#define FS_PACKAGED_BINARY_ALIAS(alias, target) \
    { "/bin/" #alias, _binary_obj_i386_user_bin_##target##_start, _binary_obj_i386_user_bin_##target##_end },
#endif
    FS_DISTINCT_USER_PROGRAMS(FS_PACKAGED_BINARY_ENTRY)
    FS_CORE_TOOL_ALIASES(FS_PACKAGED_BINARY_ALIAS)
    FS_TLS_TOOL_ALIASES(FS_PACKAGED_BINARY_ALIAS)
};

#undef FS_PACKAGED_BINARY_ALIAS
#undef FS_PACKAGED_BINARY_ENTRY
#undef FS_DECLARE_PACKAGED_BINARY

static size_t fs_packaged_binary_len(const fs_packaged_binary_t* entry) {
    return entry && entry->end >= entry->start ? (size_t)(entry->end - entry->start) : 0U;
}

static const fs_packaged_binary_t* fs_find_packaged_binary(const char* path) {
    size_t count = sizeof(fs_packaged_binaries) / sizeof(fs_packaged_binaries[0]);

    if (!path || path[0] == '\0') return 0;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(fs_packaged_binaries[i].path, path) == 0) return &fs_packaged_binaries[i];
    }
    return 0;
}

static const fs_packaged_binary_t* fs_find_packaged_binary_by_idx(int idx) {
    size_t count = sizeof(fs_packaged_binaries) / sizeof(fs_packaged_binaries[0]);

    if (idx < 0 || idx >= MAX_FILES) return 0;
    for (size_t i = 0; i < count; i++) {
        if (fs_find_node(fs_packaged_binaries[i].path) == idx) return &fs_packaged_binaries[i];
    }
    return 0;
}

static int fs_read_packaged_binary(const fs_packaged_binary_t* entry, void* buffer,
                                   size_t offset, size_t max_len) {
    uint8_t* bytes = (uint8_t*)buffer;
    size_t len = fs_packaged_binary_len(entry);
    size_t read_len;

    if (!entry) return -1;
    if (!bytes && max_len != 0U) return -1;
    if (offset >= len || max_len == 0U) return 0;

    read_len = len - offset;
    if (read_len > max_len) read_len = max_len;
    memcpy(bytes, entry->start + offset, read_len);
    return (int)read_len;
}

static uint32_t node_sector_count(const disk_fs_node_t* node) {
    uint32_t count;
    memcpy(&count, node->reserved, sizeof(count));
    if (count == 0 && node->flags == 1 && node->size > 0) count = 1;
    return count;
}

static void set_node_sector_count(disk_fs_node_t* node, uint32_t count) {
    memcpy(node->reserved, &count, sizeof(count));
}

static uint32_t node_extra_flags(const disk_fs_node_t* node) {
    uint32_t flags;

    memcpy(&flags, node->reserved + sizeof(uint32_t), sizeof(flags));
    return flags;
}

static void set_node_extra_flags(disk_fs_node_t* node, uint32_t flags) {
    memcpy(node->reserved + sizeof(uint32_t), &flags, sizeof(flags));
}

static int node_is_external_blob(const disk_fs_node_t* node) {
    return node && node->flags == FS_NODE_FILE &&
           (node_extra_flags(node) & FS_NODE_FLAG_EXTERNAL_BLOB) == FS_NODE_FLAG_EXTERNAL_BLOB;
}

static int fs_blob_matches_file(const char* path, const uint8_t* data, size_t len) {
    uint8_t verify_buffer[512];
    size_t offset = 0;
    int idx = fs_find_node(path);

    if (idx < 0 || dir_cache[idx].flags != FS_NODE_FILE) return 0;
    if (dir_cache[idx].size != len) return 0;

    while (offset < len) {
        size_t chunk = len - offset;
        int read_status;

        if (chunk > sizeof(verify_buffer)) chunk = sizeof(verify_buffer);
        read_status = fs_read_file_raw_by_idx(idx, verify_buffer, offset, chunk);
        if (read_status != (int)chunk) return 0;
        if (memcmp(verify_buffer, data + offset, chunk) != 0) return 0;
        offset += chunk;
    }
    return 1;
}

static void fs_sync_packaged_binaries() {
    size_t count = sizeof(fs_packaged_binaries) / sizeof(fs_packaged_binaries[0]);

    (void)fs_create_dir("/bin");
    (void)fs_create_dir("/assets");
    for (size_t i = 0; i < count; i++) {
        const fs_packaged_binary_t* entry = &fs_packaged_binaries[i];
        size_t len = (size_t)(entry->end - entry->start);
        int status;

        if (len == 0U) continue;
        if (fs_blob_matches_file(entry->path, entry->start, len)) continue;
        status = fs_write_file_raw(entry->path, entry->start, len);
        if (status < 0) {
            serial_write("[fs] packaged sync failed path=");
            serial_write(entry->path);
            serial_write(" len=");
            serial_write_hex32((uint32_t)len);
            serial_write(" status=");
            serial_write_hex32((uint32_t)status);
            serial_write_char('\n');
        }
    }
}

#if defined(NARCOS_DISK_DOOM1_WAD) || defined(NARCOS_DISK_DOOM_BIN)
static int fs_read_disk_blob(uint32_t src_lba, size_t len, void* buffer, size_t offset, size_t max_len) {
    uint8_t* bytes = (uint8_t*)buffer;
    size_t read_len;
    size_t copied = 0;
    uint32_t start_sector;
    size_t sector_offset;
    uint32_t sectors;
    int initrd_status;
    static int boot_initrd_logged = 0;
    static int initrd_fallback_logged = 0;

    if (!bytes && max_len != 0U) return -1;
    if (src_lba == 0U || offset >= len || max_len == 0U) return 0;

    initrd_status = fs_read_boot_initrd_blob(src_lba, len, buffer, offset, max_len);
    if (initrd_status != FS_BOOT_INITRD_MISS) {
        if (!boot_initrd_logged) {
            serial_write_line("[fs] using boot initrd memory fallback");
            boot_initrd_logged = 1;
        }
        return initrd_status;
    }
    initrd_status = fs_read_compiled_initrd_blob(src_lba, len, buffer, offset, max_len);
    if (initrd_status != FS_BOOT_INITRD_MISS) {
        if (!initrd_fallback_logged) {
            serial_write_line("[fs] using initrd memory fallback");
            initrd_fallback_logged = 1;
        }
        return initrd_status;
    }

    read_len = len - offset;
    if (read_len > max_len) read_len = max_len;
    start_sector = (uint32_t)(offset / 512U);
    sector_offset = offset % 512U;
    sectors = (uint32_t)((len + 511U) / 512U);

    for (uint32_t sector = start_sector; sector < sectors && copied < read_len; sector++) {
        size_t chunk;

        if (fs_storage_read_sector(src_lba + sector, sector_buffer) != 0) {
            serial_write("[fs] disk blob storage read failed lba=");
            serial_write_hex32(src_lba + sector);
            serial_write_char('\n');
            return -1;
        }
        chunk = 512U - sector_offset;
        if (chunk > read_len - copied) chunk = read_len - copied;
        memcpy(bytes + copied, sector_buffer + sector_offset, chunk);
        copied += chunk;
        sector_offset = 0U;
    }

    return (int)copied;
}

static int fs_disk_blob_info_for_path(const char* path, uint32_t* out_lba, size_t* out_len) {
    if (!path) return 0;
#if defined(NARCOS_DISK_DOOM_BIN)
    if (strcmp(path, "/bin/doom") == 0) {
        if (out_lba) *out_lba = (uint32_t)NARCOS_DISK_DOOM_BIN_LBA;
        if (out_len) *out_len = (size_t)NARCOS_DISK_DOOM_BIN_SIZE;
        return 1;
    }
#endif
#if defined(NARCOS_DISK_DOOM1_WAD)
    if (strcmp(path, "/assets/doom1.wad") == 0) {
        if (out_lba) *out_lba = (uint32_t)NARCOS_DISK_DOOM1_WAD_LBA;
        if (out_len) *out_len = (size_t)NARCOS_DISK_DOOM1_WAD_SIZE;
        return 1;
    }
#endif
    return 0;
}

static int fs_disk_blob_info_for_idx(int idx, uint32_t* out_lba, size_t* out_len) {
    if (idx < 0 || idx >= MAX_FILES) return 0;
#if defined(NARCOS_DISK_DOOM_BIN)
    if (idx == fs_find_node_internal("/bin/doom")) {
        if (out_lba) *out_lba = (uint32_t)NARCOS_DISK_DOOM_BIN_LBA;
        if (out_len) *out_len = (size_t)NARCOS_DISK_DOOM_BIN_SIZE;
        return 1;
    }
#endif
#if defined(NARCOS_DISK_DOOM1_WAD)
    if (idx == fs_find_node_internal("/assets/doom1.wad")) {
        if (out_lba) *out_lba = (uint32_t)NARCOS_DISK_DOOM1_WAD_LBA;
        if (out_len) *out_len = (size_t)NARCOS_DISK_DOOM1_WAD_SIZE;
        return 1;
    }
#endif
    return 0;
}

#else
static int fs_read_disk_blob(uint32_t src_lba, size_t len, void* buffer, size_t offset, size_t max_len) {
    (void)src_lba;
    (void)len;
    (void)buffer;
    (void)offset;
    (void)max_len;
    return -1;
}

static int fs_disk_blob_info_for_path(const char* path, uint32_t* out_lba, size_t* out_len) {
    (void)path;
    (void)out_lba;
    (void)out_len;
    return 0;
}

static int fs_disk_blob_info_for_idx(int idx, uint32_t* out_lba, size_t* out_len) {
    (void)idx;
    (void)out_lba;
    (void)out_len;
    return 0;
}

#endif

#if defined(NARCOS_DISK_DOOM1_WAD) || defined(NARCOS_DISK_DOOM_BIN)
static int fs_mount_disk_blob(const char* path, uint32_t src_lba, size_t len) {
    int idx;
    uint32_t sectors;

    if (!path || src_lba == 0U || len == 0U || len > MAX_FILE_SIZE) return -1;
    idx = fs_find_node(path);
    if (idx == -1) {
        if (fs_create_file(path) == -1) return -1;
        idx = fs_find_node(path);
    }
    if (idx < 0 || idx >= MAX_FILES || dir_cache[idx].flags != FS_NODE_FILE) return -1;

    sectors = (uint32_t)((len + 511U) / 512U);
    dir_cache[idx].lba = src_lba;
    dir_cache[idx].size = (uint32_t)len;
    set_node_sector_count(&dir_cache[idx], sectors);
    set_node_extra_flags(&dir_cache[idx], FS_NODE_FLAG_EXTERNAL_BLOB);
    fs_sync();
    return 0;
}

#endif

#if defined(NARCOS_DISK_DOOM_BIN)
static void fs_sync_disk_doom_binary(void) {
    int status;
    uint8_t magic[4];

    (void)fs_create_dir("/bin");
    status = fs_mount_disk_blob("/bin/doom",
                               (uint32_t)NARCOS_DISK_DOOM_BIN_LBA,
                               (size_t)NARCOS_DISK_DOOM_BIN_SIZE);
    if (status != 0) {
        serial_write("[fs] disk doom binary mount failed len=");
        serial_write_hex32((uint32_t)NARCOS_DISK_DOOM_BIN_SIZE);
        serial_write(" lba=");
        serial_write_hex32((uint32_t)NARCOS_DISK_DOOM_BIN_LBA);
        serial_write_char('\n');
        vga_print_color("[fs] doom binary mount failed\n", 0x0C);
    } else {
        vga_print_color("[fs] doom binary mounted at /bin/doom\n", 0x0A);
        if (fs_read_file_raw("/bin/doom", magic, 0U, sizeof(magic)) == (int)sizeof(magic)) {
            uint32_t value = (uint32_t)magic[0] | ((uint32_t)magic[1] << 8) |
                             ((uint32_t)magic[2] << 16) | ((uint32_t)magic[3] << 24);
            char buf[11];
            serial_write("[fs] /bin/doom magic=");
            serial_write_hex32(value);
            serial_write(value == 0x464C457FU ? " ELF\n" : " BAD\n");
            vga_print("[fs] /bin/doom magic=");
            vga_print_int_hex(value, buf);
            vga_print(buf);
            vga_println(value == 0x464C457FU ? " (ELF OK)" : " (BAD)");
        } else {
            serial_write_line("[fs] /bin/doom magic read failed");
            vga_print_color("[fs] /bin/doom magic read failed\n", 0x0C);
        }
    }
}
#endif

#if defined(NARCOS_DISK_DOOM1_WAD)
static void fs_sync_disk_doom1_wad(void) {
    int status;

    (void)fs_create_dir("/assets");
    status = fs_mount_disk_blob("/assets/doom1.wad",
                               (uint32_t)NARCOS_DISK_DOOM1_WAD_LBA,
                               (size_t)NARCOS_DISK_DOOM1_WAD_SIZE);
    if (status != 0) {
        serial_write("[fs] disk doom1.wad mount failed len=");
        serial_write_hex32((uint32_t)NARCOS_DISK_DOOM1_WAD_SIZE);
        serial_write(" lba=");
        serial_write_hex32((uint32_t)NARCOS_DISK_DOOM1_WAD_LBA);
        serial_write_char('\n');
        vga_print_color("[fs] doom1.wad mount failed\n", 0x0C);
    } else {
        vga_print_color("[fs] doom1.wad mounted at /assets/doom1.wad\n", 0x0A);
    }
}
#endif

static int fs_find_child(int parent_idx, const char* name, uint32_t type) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (dir_cache[i].flags == 0) continue;
        if (dir_cache[i].parent_index != parent_idx) continue;
        if (type != 0 && dir_cache[i].flags != type) continue;
        if (strcmp(dir_cache[i].name, name) == 0) return i;
    }
    return -1;
}

static int fs_is_valid_name(const char* name) {
    if (!name || name[0] == '\0') return 0;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;
    for (int i = 0; name[i] != '\0'; i++) {
        if (name[i] == '/') return 0;
    }
    return 1;
}

static int fs_dir_has_children(int idx) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (dir_cache[i].flags != 0 && dir_cache[i].parent_index == idx) return 1;
    }
    return 0;
}

static int fs_walk_path(const char* path, int want_parent, char* leaf_out) {
    int node = (path && path[0] == '/') ? FS_ROOT_INDEX : current_dir_index;
    int i = (path && path[0] == '/') ? 1 : 0;
    char part[32];

    if (!path || path[0] == '\0') return node;

    while (1) {
        int j = 0;
        while (path[i] == '/') i++;
        if (path[i] == '\0') {
            if (want_parent && leaf_out) leaf_out[0] = '\0';
            return node;
        }
        while (path[i] != '\0' && path[i] != '/' && j < 31) {
            part[j++] = path[i++];
        }
        part[j] = '\0';
        while (path[i] != '\0' && path[i] != '/') i++;
        while (path[i] == '/') i++;

        if (strcmp(part, ".") == 0 || part[0] == '\0') {
            continue;
        }
        if (strcmp(part, "..") == 0) {
            if (node != FS_ROOT_INDEX) node = dir_cache[node].parent_index;
            continue;
        }
        if (path[i] == '\0' && want_parent) {
            if (leaf_out) strcpy(leaf_out, part);
            return node;
        }
        node = fs_find_child(node, part, 2);
        if (node == -1) return FS_INVALID_INDEX;
    }
}

static void fs_format() {
    for (int i = 0; i < MAX_FILES; i++) {
        dir_cache[i].flags = 0;
        dir_cache[i].size = 0;
        dir_cache[i].parent_index = FS_ROOT_INDEX;
        dir_cache[i].lba = 0;
        dir_cache[i].name[0] = '\0';
        memset(dir_cache[i].reserved, 0, sizeof(dir_cache[i].reserved));
    }

    current_dir_index = FS_ROOT_INDEX;
    fs_create_dir("/bin");
    fs_create_dir("/assets");
    fs_create_dir("/system");
    fs_create_dir("/home");
    fs_create_dir("/home/user");
    fs_create_dir("/home/user/Desktop");
    fs_write_file("/home/user/Desktop/readme.txt",
                  "Welcome to NarcOs Professional Desktop!\n"
                  "Files here appear on your desktop icons.\n");
    current_dir_index = FS_ROOT_INDEX;
    fs_sync();
}

static int fs_validate() {
    for (int i = 0; i < MAX_FILES; i++) {
        if (dir_cache[i].flags == 0) continue;
        if (dir_cache[i].flags != FS_NODE_FILE && dir_cache[i].flags != FS_NODE_DIR) return 0;
        if (!fs_is_valid_name(dir_cache[i].name)) return 0;
        if (dir_cache[i].parent_index < FS_ROOT_INDEX || dir_cache[i].parent_index >= MAX_FILES) return 0;
        if (dir_cache[i].parent_index >= 0 && dir_cache[dir_cache[i].parent_index].flags != FS_NODE_DIR) return 0;
        if (dir_cache[i].flags == FS_NODE_FILE) {
            uint32_t sectors = node_sector_count(&dir_cache[i]);
            if (dir_cache[i].size > MAX_FILE_SIZE) return 0;
            if (dir_cache[i].size == 0 && sectors != 0) return 0;
            if (dir_cache[i].size > 0 && sectors == 0) return 0;
            if (!node_is_external_blob(&dir_cache[i]) &&
                dir_cache[i].lba != 0 &&
                (dir_cache[i].lba < DATA_START_SECTOR || dir_cache[i].lba + sectors > DATA_END_SECTOR)) {
                return 0;
            }
        }
        int slow = i;
        while (slow >= 0) {
            slow = dir_cache[slow].parent_index;
            if (slow == i) return 0;
        }
        for (int j = i + 1; j < MAX_FILES; j++) {
            if (dir_cache[j].flags == 0) continue;
            if (dir_cache[j].parent_index == dir_cache[i].parent_index && strcmp(dir_cache[j].name, dir_cache[i].name) == 0) {
                return 0;
            }
        }
    }

    if (fs_find_child(FS_ROOT_INDEX, "home", FS_NODE_DIR) == -1) return 0;
    if (fs_find_node("/home/user/Desktop") == -1) return 0;
    return 1;
}

static int fs_alloc_data_run(uint32_t sectors, int ignore_idx) {
    if (sectors == 0) return 0;
    uint32_t run = 0;
    uint32_t start = DATA_START_SECTOR;
    for (uint32_t lba = DATA_START_SECTOR; lba < DATA_END_SECTOR; lba++) {
        int used = 0;
        for (int i = 0; i < MAX_FILES; i++) {
            if (i == ignore_idx || dir_cache[i].flags != 1) continue;
            uint32_t node_lba = dir_cache[i].lba;
            uint32_t node_secs = node_sector_count(&dir_cache[i]);
            if (node_secs == 0) continue;
            if (lba >= node_lba && lba < node_lba + node_secs) {
                used = 1;
                break;
            }
        }
        if (!used) {
            if (run == 0) start = lba;
            run++;
            if (run == sectors) return (int)start;
        } else {
            run = 0;
        }
    }
    return -1;
}

static void fs_zero_sectors(uint32_t lba, uint32_t count) {
    memset(sector_buffer, 0, sizeof(sector_buffer));
    for (uint32_t i = 0; i < count; i++) {
        (void)fs_storage_write_sector(lba + i, sector_buffer);
    }
}

void fs_sync() {
    for (int i = 0; i < DIR_SECTOR_COUNT; i++) {
        (void)fs_storage_write_sector(DIR_SECTOR + (uint32_t)i, (uint8_t*)dir_cache + (i * 512));
    }
}
static void load_dir_cache() {
    for (int i = 0; i < DIR_SECTOR_COUNT; i++) {
        if (fs_storage_read_sector(DIR_SECTOR + (uint32_t)i, (uint8_t*)dir_cache + (i * 512)) != 0) {
            memset((uint8_t*)dir_cache + (i * 512), 0, 512);
        }
    }
}
void init_fs() {
#if UINTPTR_MAX > 0xFFFFFFFFU
    memset(dir_cache, 0, sizeof(dir_cache));
    current_dir_index = FS_ROOT_INDEX;
    fs_format();
    serial_write("[fs] post-format /bin=");
    serial_write_hex32((uint32_t)fs_find_node("/bin"));
    serial_write(" /home=");
    serial_write_hex32((uint32_t)fs_find_node("/home"));
    serial_write(" /home/user/Desktop=");
    serial_write_hex32((uint32_t)fs_find_node("/home/user/Desktop"));
    serial_write_char('\n');
    fs_sync_packaged_binaries();
#if defined(NARCOS_DISK_DOOM_BIN)
    fs_sync_disk_doom_binary();
#endif
#if defined(NARCOS_DISK_DOOM1_WAD)
    fs_sync_disk_doom1_wad();
#endif
    return;
#endif

    load_dir_cache();
    current_dir_index = FS_ROOT_INDEX;
    if (!fs_validate()) fs_format();
    fs_sync_packaged_binaries();
#if defined(NARCOS_DISK_DOOM_BIN)
    fs_sync_disk_doom_binary();
#endif
#if defined(NARCOS_DISK_DOOM1_WAD)
    fs_sync_disk_doom1_wad();
#endif
}
int fs_create_file(const char* name) {
    char leaf[32];
    leaf[0] = '\0';
    int parent = fs_walk_path(name, 1, leaf);
    if (parent == FS_INVALID_INDEX || leaf[0] == '\0' || !fs_is_valid_name(leaf)) {
        serial_write("[fs] create_file invalid path=");
        serial_write(name ? name : "<null>");
        serial_write(" parent=");
        serial_write_hex32((uint32_t)parent);
        serial_write(" leaf=");
        serial_write(leaf);
        serial_write_char('\n');
        return -1;
    }
    if (fs_find_child(parent, leaf, 0) != -1) {
        serial_write("[fs] create_file exists path=");
        serial_write(name ? name : "<null>");
        serial_write(" parent=");
        serial_write_hex32((uint32_t)parent);
        serial_write(" leaf=");
        serial_write(leaf);
        serial_write_char('\n');
        return -1;
    }
    for (int i = 0; i < MAX_FILES; i++) {
        if (dir_cache[i].flags == 0) {
            strncpy(dir_cache[i].name, leaf, 31);
            dir_cache[i].name[31] = '\0';
            dir_cache[i].size = 0;
            dir_cache[i].flags = FS_NODE_FILE;
            dir_cache[i].parent_index = parent;
            dir_cache[i].lba = 0;
            memset(dir_cache[i].reserved, 0, sizeof(dir_cache[i].reserved));
            fs_sync();
            return 0;
        }
    }
    serial_write("[fs] create_file full path=");
    serial_write(name ? name : "<null>");
    serial_write_char('\n');
    return -1;
}
int fs_create_dir(const char* name) {
    char leaf[32];
    leaf[0] = '\0';
    int parent = fs_walk_path(name, 1, leaf);
    if (parent == FS_INVALID_INDEX || leaf[0] == '\0' || !fs_is_valid_name(leaf)) {
        serial_write("[fs] create_dir invalid path=");
        serial_write(name ? name : "<null>");
        serial_write(" parent=");
        serial_write_hex32((uint32_t)parent);
        serial_write(" leaf=");
        serial_write(leaf);
        serial_write_char('\n');
        return -1;
    }
    if (fs_find_child(parent, leaf, 0) != -1) {
        serial_write("[fs] create_dir exists path=");
        serial_write(name ? name : "<null>");
        serial_write(" parent=");
        serial_write_hex32((uint32_t)parent);
        serial_write(" leaf=");
        serial_write(leaf);
        serial_write_char('\n');
        return -1;
    }
    for (int i = 0; i < MAX_FILES; i++) {
        if (dir_cache[i].flags == 0) {
            strncpy(dir_cache[i].name, leaf, 31);
            dir_cache[i].name[31] = '\0';
            dir_cache[i].size = 0;
            dir_cache[i].flags = FS_NODE_DIR;
            dir_cache[i].parent_index = parent;
            dir_cache[i].lba = 0;
            memset(dir_cache[i].reserved, 0, sizeof(dir_cache[i].reserved));
            fs_sync();
            return 0;
        }
    }
    serial_write("[fs] create_dir full path=");
    serial_write(name ? name : "<null>");
    serial_write_char('\n');
    return -1;
}
int fs_change_dir(const char* name) {
    int idx = fs_walk_path(name, 0, 0);
    if (idx != FS_INVALID_INDEX) {
        current_dir_index = idx;
        return 0;
    }
    return -1;
}
int fs_write_file_raw_by_idx(int idx, const void* data, size_t len) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t needed_sectors;
    uint32_t current_sectors;
    uint32_t old_lba;
    int moved = 0;

    if (idx < 0 || idx >= MAX_FILES || dir_cache[idx].flags != FS_NODE_FILE) return -1;
    if (node_is_external_blob(&dir_cache[idx])) return -1;
    if (!bytes && len != 0U) return -1;
    if (len > MAX_FILE_SIZE) len = MAX_FILE_SIZE;
    current_sectors = node_sector_count(&dir_cache[idx]);
    old_lba = dir_cache[idx].lba;
    needed_sectors = (uint32_t)((len + 511U) / 512U);

    if (needed_sectors != 0) {
        if (dir_cache[idx].lba == 0 || current_sectors < needed_sectors) {
            int new_lba = fs_alloc_data_run(needed_sectors, idx);
            if (new_lba == -1) return -1;
            dir_cache[idx].lba = (uint32_t)new_lba;
            moved = (old_lba != 0 && old_lba != dir_cache[idx].lba);
        }
    } else {
        dir_cache[idx].lba = 0;
    }

    dir_cache[idx].size = (uint32_t)len;
    set_node_sector_count(&dir_cache[idx], needed_sectors);
    fs_sync();
    if (needed_sectors == 0) {
        if (old_lba != 0 && current_sectors != 0) fs_zero_sectors(old_lba, current_sectors);
        return 0;
    }

    for (uint32_t sector = 0; sector < needed_sectors; sector++) {
        size_t offset = sector * 512U;
        size_t chunk = len > offset ? len - offset : 0;
        if (chunk > 512U) chunk = 512U;
        memset(sector_buffer, 0, sizeof(sector_buffer));
        memcpy(sector_buffer, bytes + offset, chunk);
        if (fs_storage_write_sector(dir_cache[idx].lba + sector, sector_buffer) != 0) return -1;
    }
    if (current_sectors > needed_sectors && dir_cache[idx].lba != 0) {
        fs_zero_sectors(dir_cache[idx].lba + needed_sectors, current_sectors - needed_sectors);
    }
    if (moved && old_lba != 0) fs_zero_sectors(old_lba, current_sectors);
    return (int)len;
}

int fs_write_file_by_idx(int idx, const char* data) {
    size_t len;

    if (idx < 0 || idx >= MAX_FILES || dir_cache[idx].flags != FS_NODE_FILE || !data) return -1;
    len = strlen(data);
    if (len > MAX_TEXT_FILE_SIZE) len = MAX_TEXT_FILE_SIZE;
    return fs_write_file_raw_by_idx(idx, data, len) < 0 ? -1 : 0;
}
int fs_write_file_raw_at_by_idx(int idx, const void* data, size_t offset, size_t len) {
    uint8_t* merged;
    size_t old_size;
    size_t new_size;
    size_t write_len;
    int read_status;
    int write_status;

    if (idx < 0 || idx >= MAX_FILES || dir_cache[idx].flags != FS_NODE_FILE) return -1;
    if (node_is_external_blob(&dir_cache[idx])) return -1;
    if (!data && len != 0U) return -1;
    if (offset > MAX_FILE_SIZE) return -1;
    if (len > MAX_FILE_SIZE - offset) len = MAX_FILE_SIZE - offset;
    if (len == 0U) return 0;
    write_len = len;
    old_size = dir_cache[idx].size;
    new_size = old_size > offset + len ? old_size : offset + len;
    if (new_size == 0U) return fs_write_file_raw_by_idx(idx, 0, 0U);

    merged = (uint8_t*)malloc(new_size);
    if (!merged) return -1;
    memset(merged, 0, new_size);
    if (old_size != 0U) {
        read_status = fs_read_file_raw_by_idx(idx, merged, 0U, old_size);
        if (read_status < 0) {
            free(merged);
            return -1;
        }
    }
    if (len != 0U) memcpy(merged + offset, data, len);
    write_status = fs_write_file_raw_by_idx(idx, merged, new_size);
    free(merged);
    return write_status < 0 ? write_status : (int)write_len;
}
int fs_write_file(const char* name, const char* data) {
    int idx = fs_find_node(name);
    if (idx == -1) {
        if (fs_create_file(name) == -1) return -1;
        idx = fs_find_node(name);
    }
    if (idx == -1 || dir_cache[idx].flags != FS_NODE_FILE) return -1;
    return fs_write_file_by_idx(idx, data);
}
int fs_read_file_raw_by_idx(int idx, void* buffer, size_t offset, size_t max_len) {
    const fs_packaged_binary_t* packaged;
    uint8_t* bytes = (uint8_t*)buffer;
    size_t read_len;
    uint32_t sectors;
    size_t copied = 0;
    uint32_t start_sector;
    size_t sector_offset;
    uint32_t blob_lba;
    size_t blob_len;

    if (idx < 0 || idx >= MAX_FILES || dir_cache[idx].flags != FS_NODE_FILE) return -1;
    packaged = fs_find_packaged_binary_by_idx(idx);
    if (packaged) return fs_read_packaged_binary(packaged, buffer, offset, max_len);
    if (fs_disk_blob_info_for_idx(idx, &blob_lba, &blob_len)) {
        return fs_read_disk_blob(blob_lba, blob_len, buffer, offset, max_len);
    }
    if (!bytes && max_len != 0U) return -1;
    if (offset >= dir_cache[idx].size || max_len == 0U) return 0;

    sectors = node_sector_count(&dir_cache[idx]);
    read_len = dir_cache[idx].size - offset;
    if (read_len > max_len) read_len = max_len;
    start_sector = (uint32_t)(offset / 512U);
    sector_offset = offset % 512U;

    for (uint32_t sector = start_sector; sector < sectors && copied < read_len; sector++) {
        size_t chunk;

        if (fs_storage_read_sector(dir_cache[idx].lba + sector, sector_buffer) != 0) return -1;
        chunk = 512U - sector_offset;
        if (chunk > read_len - copied) chunk = read_len - copied;
        memcpy(bytes + copied, sector_buffer + sector_offset, chunk);
        copied += chunk;
        sector_offset = 0U;
    }

    return (int)copied;
}
int fs_read_file_by_idx(int idx, char* buffer, size_t max_len) {
    int read_len;

    if (idx < 0 || idx >= MAX_FILES || dir_cache[idx].flags != FS_NODE_FILE || !buffer || max_len == 0) return -1;
    read_len = fs_read_file_raw_by_idx(idx, buffer, 0U, max_len - 1U);
    if (read_len < 0) return -1;
    buffer[read_len] = '\0';
    return 0;
}
int fs_write_file_raw(const char* name, const void* data, size_t len) {
    int idx = fs_find_node(name);
    int status;

    if (idx == -1) {
        if (fs_create_file(name) == -1) {
            serial_write("[fs] write_raw create failed path=");
            serial_write(name ? name : "<null>");
            serial_write_char('\n');
            return -1;
        }
        idx = fs_find_node(name);
    }
    if (idx == -1 || dir_cache[idx].flags != FS_NODE_FILE) {
        serial_write("[fs] write_raw lookup failed path=");
        serial_write(name ? name : "<null>");
        serial_write(" idx=");
        serial_write_hex32((uint32_t)idx);
        serial_write(" flags=");
        serial_write_hex32((uint32_t)(idx >= 0 ? dir_cache[idx].flags : 0));
        serial_write_char('\n');
        return -1;
    }
    status = fs_write_file_raw_by_idx(idx, data, len);
    if (status < 0) {
        serial_write("[fs] write_raw body failed path=");
        serial_write(name ? name : "<null>");
        serial_write(" idx=");
        serial_write_hex32((uint32_t)idx);
        serial_write(" len=");
        serial_write_hex32((uint32_t)len);
        serial_write_char('\n');
    }
    return status;
}
int fs_write_file_raw_at(const char* name, const void* data, size_t offset, size_t len) {
    int idx = fs_find_node(name);

    if (idx == -1) {
        if (fs_create_file(name) == -1) return -1;
        idx = fs_find_node(name);
    }
    if (idx == -1 || dir_cache[idx].flags != FS_NODE_FILE) return -1;
    return fs_write_file_raw_at_by_idx(idx, data, offset, len);
}
int fs_read_file(const char* name, char* buffer, size_t max_len) {
    int idx = fs_find_node(name);
    if (idx == -1) return -1;
    return fs_read_file_by_idx(idx, buffer, max_len);
}
int fs_read_file_raw(const char* name, void* buffer, size_t offset, size_t max_len) {
    const fs_packaged_binary_t* packaged = fs_find_packaged_binary(name);
    int idx = fs_find_node(name);
    uint32_t blob_lba;
    size_t blob_len;

    if (packaged) return fs_read_packaged_binary(packaged, buffer, offset, max_len);
    if (fs_disk_blob_info_for_path(name, &blob_lba, &blob_len)) {
        return fs_read_disk_blob(blob_lba, blob_len, buffer, offset, max_len);
    }
    if (idx == -1) return -1;
    return fs_read_file_raw_by_idx(idx, buffer, offset, max_len);
}
int fs_delete_file(const char* name) {
    int idx = fs_find_node(name);
    if (idx < 0) return -1;
    if (idx >= 0 && dir_cache[idx].flags == FS_NODE_DIR && fs_dir_has_children(idx)) return -1;
    if (idx >= 0 && dir_cache[idx].flags == FS_NODE_FILE) {
        uint32_t sectors = node_sector_count(&dir_cache[idx]);
        if (!node_is_external_blob(&dir_cache[idx]) &&
            dir_cache[idx].lba != 0 && sectors != 0) {
            fs_zero_sectors(dir_cache[idx].lba, sectors);
        }
    }
    dir_cache[idx].flags = 0;
    dir_cache[idx].size = 0;
    dir_cache[idx].lba = 0;
    dir_cache[idx].parent_index = FS_ROOT_INDEX;
    dir_cache[idx].name[0] = '\0';
    memset(dir_cache[idx].reserved, 0, sizeof(dir_cache[idx].reserved));
    fs_sync();
    return 0;
}
int fs_move_file(const char* name, const char* target_dir) {
    int file_idx = fs_find_node(name);
    if (file_idx == -1) return -1;

    int target_idx = fs_walk_path(target_dir, 0, 0);
    if (target_idx == FS_INVALID_INDEX) return -1;
    if (dir_cache[file_idx].flags == FS_NODE_DIR) {
        int walker = target_idx;
        while (walker >= 0) {
            if (walker == file_idx) return -1;
            walker = dir_cache[walker].parent_index;
        }
    }
    if (fs_find_child(target_idx, dir_cache[file_idx].name, 0) != -1) return -1;

    dir_cache[file_idx].parent_index = target_idx;
    fs_sync();
    return 0;
}
int fs_rename(const char* path, const char* new_name) {
    int idx = fs_find_node(path);
    if (idx == -1 || !fs_is_valid_name(new_name)) return -1;
    if (fs_find_child(dir_cache[idx].parent_index, new_name, 0) != -1) return -1;
    strncpy(dir_cache[idx].name, new_name, 31);
    dir_cache[idx].name[31] = '\0';
    fs_sync();
    return 0;
}
void fs_list_dir() {
    vga_print_color("Name\t\tSize (Bytes)\n", 0x07);
    vga_print_color("----------------------------\n", 0x08);
    for (int i = 0; i < MAX_FILES; i++) {
        if (dir_cache[i].flags != 0 && dir_cache[i].parent_index == current_dir_index) {
            if (dir_cache[i].flags == 2) {
                vga_print_color(dir_cache[i].name, 0x0A);
                vga_println("/\t\t<DIR>");
            } else {
                vga_print_color(dir_cache[i].name, 0x0B);
                vga_print("\t");
                if (strlen(dir_cache[i].name) < 8) {
                    vga_print("\t");
                }
                vga_print_int(dir_cache[i].size);
                vga_println("");
            }
        }
    }
}

int fs_list_dir_entries(disk_fs_node_t* out_entries, int max_entries) {
    int count = 0;

    if (!out_entries || max_entries <= 0) return -1;
    for (int i = 0; i < MAX_FILES && count < max_entries; i++) {
        if (dir_cache[i].flags == 0 || dir_cache[i].parent_index != current_dir_index) continue;
        out_entries[count++] = dir_cache[i];
    }
    return count;
}

int fs_get_node_info(int idx, disk_fs_node_t* out_node) {
    const fs_packaged_binary_t* packaged;
    uint32_t blob_lba;
    size_t blob_len;

    if (!out_node) return -1;
    if (idx < 0 || idx >= MAX_FILES) return -1;
    if (dir_cache[idx].flags == 0) return -1;
    *out_node = dir_cache[idx];
    if (fs_disk_blob_info_for_idx(idx, &blob_lba, &blob_len)) {
        uint32_t sectors = (uint32_t)((blob_len + 511U) / 512U);

        out_node->flags = FS_NODE_FILE;
        out_node->lba = blob_lba;
        out_node->size = (uint32_t)blob_len;
        memcpy(out_node->reserved, &sectors, sizeof(sectors));
        set_node_extra_flags(out_node, FS_NODE_FLAG_EXTERNAL_BLOB);
        return 0;
    }
    packaged = fs_find_packaged_binary_by_idx(idx);
    if (packaged) {
        uint32_t sectors;
        size_t len = fs_packaged_binary_len(packaged);

        out_node->flags = FS_NODE_FILE;
        out_node->size = (uint32_t)len;
        sectors = (uint32_t)((len + 511U) / 512U);
        memcpy(out_node->reserved, &sectors, sizeof(sectors));
    }
    return 0;
}

void get_current_dir_name(char* buf) {
    if (current_dir_index == FS_ROOT_INDEX) {
        buf[0] = '\0';
    } else {
        strncpy(buf, dir_cache[current_dir_index].name, 31);
        buf[31] = '\0';
    }
}

static int fs_find_node_internal(const char* path) {
    if (!path || path[0] == '\0') return -1;
    if (strcmp(path, "/") == 0) return FS_ROOT_INDEX;
    if (strcmp(path, ".") == 0) return current_dir_index;
    if (strcmp(path, "..") == 0) return current_dir_index == FS_ROOT_INDEX ? FS_ROOT_INDEX : dir_cache[current_dir_index].parent_index;

    char leaf[32];
    int parent = fs_walk_path(path, 1, leaf);
    if (parent == FS_INVALID_INDEX || leaf[0] == '\0') return -1;
    return fs_find_child(parent, leaf, 0);
}

int fs_find_node(const char* path) {
    return fs_find_node_internal(path);
}

void fs_get_path_by_index(int idx, char* buf, size_t max_len) {
    char temp[256];
    int segments[32];
    int count = 0;

    if (!buf || max_len == 0) return;
    buf[0] = '\0';
    if (idx == FS_ROOT_INDEX) {
        if (max_len >= 2) {
            buf[0] = '/';
            buf[1] = '\0';
        }
        return;
    }
    if (idx < 0 || idx >= MAX_FILES || dir_cache[idx].flags == 0) return;

    while (idx >= 0 && count < 32) {
        segments[count++] = idx;
        idx = dir_cache[idx].parent_index;
    }

    temp[0] = '\0';
    for (int i = count - 1; i >= 0; i--) {
        size_t len = strlen(temp);
        if (len + 1 >= sizeof(temp)) break;
        temp[len] = '/';
        temp[len + 1] = '\0';
        len++;
        size_t j = 0;
        while (dir_cache[segments[i]].name[j] != '\0' && len + j + 1 < sizeof(temp)) {
            temp[len + j] = dir_cache[segments[i]].name[j];
            j++;
        }
        temp[len + j] = '\0';
    }

    if (temp[0] == '\0') {
        if (max_len >= 2) {
            buf[0] = '/';
            buf[1] = '\0';
        }
        return;
    }

    strncpy(buf, temp, max_len - 1);
    buf[max_len - 1] = '\0';
}

void fs_get_current_path(char* buf, size_t max_len) {
    char temp[256];
    int segments[32];
    int count = 0;

    if (!buf || max_len == 0) return;
    buf[0] = '\0';
    if (current_dir_index == FS_ROOT_INDEX) {
        if (max_len >= 2) {
            buf[0] = '/';
            buf[1] = '\0';
        }
        return;
    }

    int node = current_dir_index;
    while (node >= 0 && count < 32) {
        segments[count++] = node;
        node = dir_cache[node].parent_index;
    }

    temp[0] = '\0';
    for (int i = count - 1; i >= 0; i--) {
        size_t len = strlen(temp);
        if (len + 1 >= sizeof(temp)) break;
        temp[len] = '/';
        temp[len + 1] = '\0';
        len++;
        size_t j = 0;
        while (dir_cache[segments[i]].name[j] != '\0' && len + j + 1 < sizeof(temp)) {
            temp[len + j] = dir_cache[segments[i]].name[j];
            j++;
        }
        temp[len + j] = '\0';
    }

    if (temp[0] == '\0') {
        if (max_len >= 2) {
            buf[0] = '/';
            buf[1] = '\0';
        }
    } else {
        strncpy(buf, temp, max_len - 1);
        buf[max_len - 1] = '\0';
    }
}
