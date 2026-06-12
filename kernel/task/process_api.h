#ifndef PROCESS_API_H
#define PROCESS_API_H

#include <stdint.h>

#define PROCESS_SNAPSHOT_NAME_LEN 32
#define PROCESS_SNAPSHOT_IMAGE_LEN 128

typedef struct {
    int pid;
    int parent_pid;
    int state;
    int kind;
    int exit_code;
    uint32_t flags;
    uint32_t memory_bytes;
    char name[PROCESS_SNAPSHOT_NAME_LEN];
    char image_path[PROCESS_SNAPSHOT_IMAGE_LEN];
} process_snapshot_entry_t;

typedef struct {
    uint32_t size;
    uint64_t installed_memory_bytes;
    uint64_t total_memory_bytes;
    uint64_t used_memory_bytes;
    uint64_t free_memory_bytes;
    uint32_t process_count;
    uint32_t uptime_ticks;
} system_info_t;

#endif
