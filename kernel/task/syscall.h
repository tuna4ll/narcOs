#ifndef SYSCALL_H
#define SYSCALL_H

#include <stddef.h>
#include <stdint.h>
#include "arch.h"
#include "gui_events.h"
#include "process_api.h"

#define PRIV_CMD_SNAKE 1
#define PRIV_CMD_SETTINGS 2
#define PRIV_CMD_EDIT 3
#define PRIV_CMD_MEM 4
#define PRIV_CMD_MALLOC_TEST 5
#define PRIV_CMD_USERMODE_TEST 6
#define PRIV_CMD_HWINFO 7
#define PRIV_CMD_PCI 8
#define PRIV_CMD_STORAGE 9
#define PRIV_CMD_LOG 10
#define PRIV_CMD_REBOOT 11
#define PRIV_CMD_POWEROFF 12
#define PRIV_CMD_PROC_DUMP 13
#define PRIV_CMD_PROC_TEST 14
#define PRIV_CMD_PIPE_TEST 15
#define PRIV_CMD_EXPLORER 16

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtc_local_time_t;

typedef struct {
    uint32_t size;
    int32_t x;
    int32_t y;
    int32_t dx;
    int32_t dy;
    int32_t wheel;
    uint32_t buttons;
} mouse_state_t;

#define WAITPID_FLAG_NOHANG 0x1U

#define SYS_EXIT    0
#define SYS_PRINT   1
#define SYS_MALLOC  2
#define SYS_FREE    3
#define SYS_GUI_UPDATE 4
#define SYS_YIELD   5
#define SYS_UPTIME  6
#define SYS_GETPID  7
#define SYS_CHDIR   8
#define SYS_FS_READ 9
#define SYS_FS_WRITE 10
#define SYS_SNAKE_GET_INPUT 11
#define SYS_SNAKE_CLOSE 12
#define SYS_RANDOM 13
#define SYS_NET_GET_CONFIG 14
#define SYS_NET_RESOLVE 15
#define SYS_NET_NTP_QUERY 16
#define SYS_NET_SOCKET_OPEN 17
#define SYS_NET_SOCKET_CONNECT 18
#define SYS_NET_SOCKET_SEND 19
#define SYS_NET_SOCKET_RECV 20
#define SYS_NET_SOCKET_AVAILABLE 21
#define SYS_NET_SOCKET_CLOSE 22
#define SYS_FS_LIST 23
#define SYS_FS_GET_CWD 24
#define SYS_FS_TOUCH 25
#define SYS_FS_MKDIR 26
#define SYS_FS_DELETE 27
#define SYS_FS_MOVE 28
#define SYS_FS_RENAME 29
#define SYS_CLEAR_SCREEN 30
#define SYS_RTC_GET_LOCAL 31
#define SYS_NET_DHCP 32
#define SYS_NET_PING 33
#define SYS_PRIV_CMD 34
#define SYS_PRINT_RAW 35
#define SYS_FS_FIND_NODE 36
#define SYS_FS_GET_NODE_INFO 37
#define SYS_FS_GET_PATH 38
#define SYS_RTC_GET_TZ_OFFSET 39
#define SYS_RTC_SET_TZ_OFFSET 40
#define SYS_RTC_SAVE_TZ 41
#define SYS_GUI_OPEN_NARCPAD_FILE 42
#define SYS_GETRANDOM 43
#define SYS_FS_READ_RAW 44
#define SYS_FS_WRITE_RAW 45
#define SYS_SPAWN 46
#define SYS_EXEC 47
#define SYS_WAITPID 48
#define SYS_KILL 49
#define SYS_GETPPID 50
#define SYS_SLEEP 51
#define SYS_READ 52
#define SYS_WRITE 53
#define SYS_CLOSE 54
#define SYS_DUP2 55
#define SYS_PIPE 56
#define SYS_PROCESS_SNAPSHOT 57
#define SYS_GUI_CREATE_WINDOW 58
#define SYS_GUI_DESTROY_WINDOW 59
#define SYS_GUI_SET_TITLE 60
#define SYS_GUI_POLL_EVENT 61
#define SYS_GUI_PRESENT 62
#define SYS_GUI_GET_WINDOW_INFO 63
#define SYS_GUI_GET_SCREEN_INFO 64
#define SYS_GUI_REGISTER_DESKTOP 65
#define SYS_GUI_POLL_DESKTOP_EVENT 66
#define SYS_GUI_LIST_WINDOWS 67
#define SYS_GUI_DESKTOP_WINDOW_ACTION 68
#define SYS_GUI_READ_WINDOW_SURFACE 69
#define SYS_GUI_DESKTOP_CONSUME_OPEN_PATH 70
#define SYS_NET_GET_STATS 71
#define SYS_MOUSE_GET_STATE 72
#define SYS_GUI_SET_INPUT_CAPTURE 73
#define SYS_OPENAT 74
#define SYS_LSEEK 75
#define SYS_FSTAT 76
#define SYS_STAT 77
#define SYS_BRK 78
#define SYS_CLOCK_GETTIME 79
#define SYS_NANOSLEEP 80
#define SYS_GETDENTS64 81
#define SYS_UNLINKAT 82
#define SYS_RENAMEAT 83
#define SYS_MKDIRAT 84
#define SYS_FSTATAT 85
#define SYS_FACCESSAT 86
#define SYS_FCNTL 87
#define SYS_IOCTL 88
#define SYS_DUP 89
#define SYS_SET_THREAD_AREA 90
#define SYS_MMAP 91
#define SYS_MUNMAP 92
#define SYS_MPROTECT 93

#define NARCOS_AT_FDCWD (-100)

#define NARCOS_O_RDONLY 0x0000U
#define NARCOS_O_WRONLY 0x0001U
#define NARCOS_O_RDWR   0x0002U
#define NARCOS_O_CREAT  0x0040U
#define NARCOS_O_TRUNC  0x0200U
#define NARCOS_O_APPEND 0x0400U
#define NARCOS_O_NONBLOCK 04000U
#define NARCOS_O_DIRECTORY 0200000U
#define NARCOS_O_CLOEXEC 02000000U

#define NARCOS_SEEK_SET 0
#define NARCOS_SEEK_CUR 1
#define NARCOS_SEEK_END 2

#define NARCOS_AT_SYMLINK_NOFOLLOW 0x100U
#define NARCOS_AT_REMOVEDIR 0x200U

#define NARCOS_F_DUPFD 0
#define NARCOS_F_GETFD 1
#define NARCOS_F_SETFD 2
#define NARCOS_F_GETFL 3
#define NARCOS_F_SETFL 4
#define NARCOS_FD_CLOEXEC 1

#define NARCOS_TIOCGWINSZ 0x5413U

#define NARCOS_PROT_NONE  0x0U
#define NARCOS_PROT_READ  0x1U
#define NARCOS_PROT_WRITE 0x2U
#define NARCOS_PROT_EXEC  0x4U

#define NARCOS_MAP_PRIVATE   0x02U
#define NARCOS_MAP_FIXED     0x10U
#define NARCOS_MAP_ANONYMOUS 0x20U

#define NARCOS_S_IFREG 0100000U
#define NARCOS_S_IFDIR 0040000U
#define NARCOS_S_IRUSR 0000400U
#define NARCOS_S_IWUSR 0000200U
#define NARCOS_S_IXUSR 0000100U
#define NARCOS_S_IRGRP 0000040U
#define NARCOS_S_IWGRP 0000020U
#define NARCOS_S_IXGRP 0000010U
#define NARCOS_S_IROTH 0000004U
#define NARCOS_S_IWOTH 0000002U
#define NARCOS_S_IXOTH 0000001U

typedef struct {
    uint32_t size;
    uint32_t mode;
    uint32_t nlink;
    uint32_t uid;
    uint32_t gid;
    uint32_t rdev;
    uint64_t ino;
    uint64_t dev;
    uint64_t file_size;
    uint64_t blksize;
    uint64_t blocks;
    int64_t atime_sec;
    int64_t atime_nsec;
    int64_t mtime_sec;
    int64_t mtime_nsec;
    int64_t ctime_sec;
    int64_t ctime_nsec;
} narcos_stat_t;

typedef struct {
    int64_t tv_sec;
    int64_t tv_nsec;
} narcos_timespec_t;

void syscall_handler(arch_trap_frame_t* frame);
int copy_from_user(void* dst, const void* user_src, uint32_t len);
int copy_to_user(void* user_dst, const void* src, uint32_t len);
int copy_string_from_user(char* dst, const char* user_src, size_t dst_size);

#endif
