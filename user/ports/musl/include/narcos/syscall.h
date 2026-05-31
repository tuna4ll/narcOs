#ifndef NARCOS_MUSL_SYSCALL_H
#define NARCOS_MUSL_SYSCALL_H

#define SYS_exit 0
#define SYS_getpid 7
#define SYS_chdir 8
#define SYS_getrandom 43
#define SYS_spawn 46
#define SYS_exec 47
#define SYS_waitpid 48
#define SYS_kill 49
#define SYS_getppid 50
#define SYS_sleep 51
#define SYS_read 52
#define SYS_write 53
#define SYS_close 54
#define SYS_dup2 55
#define SYS_pipe 56
#define SYS_openat 74
#define SYS_lseek 75
#define SYS_fstat 76
#define SYS_stat 77
#define SYS_brk 78
#define SYS_clock_gettime 79
#define SYS_nanosleep 80
#define SYS_getdents64 81
#define SYS_unlinkat 82
#define SYS_renameat 83
#define SYS_mkdirat 84
#define SYS_fstatat 85
#define SYS_faccessat 86
#define SYS_fcntl 87
#define SYS_ioctl 88
#define SYS_dup 89
#define SYS_set_thread_area 90
#define SYS_mmap 91
#define SYS_munmap 92
#define SYS_mprotect 93

#define AT_FDCWD (-100)

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR   0x0002
#define O_CREAT  0x0040
#define O_TRUNC  0x0200
#define O_APPEND 0x0400
#define O_NONBLOCK 04000
#define O_DIRECTORY 0200000
#define O_CLOEXEC 02000000

#endif
