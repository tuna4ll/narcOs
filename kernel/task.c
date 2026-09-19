#include <kernel/console.h>
#include <kernel/task.h>
#include <kernel/string.h>
#include <kernel/vfs.h>

#define TASK_MAX 2
#define IA32_FS_BASE 0xc0000100u
#define USER_MMAP_BASE 0x0000100010000000ULL
#define TASK_FD_MAX 16

struct task {
    int pid;
    int runnable;
    struct address_space space;
    struct task_frame frame;
    uint64_t fs_base;
    uint64_t mmap_next;
    uint8_t fpu[512] __attribute__((aligned(16)));
    struct file files[TASK_FD_MAX];
    uint8_t fd_used[TASK_FD_MAX];
};

static struct task tasks[TASK_MAX];
static struct task *current;

extern void task_enter(struct task_frame *frame) __attribute__((noreturn));

static void set_fs(uint64_t value) {
    __asm__ volatile ("wrmsr" : : "c"(IA32_FS_BASE), "a"((uint32_t)value),
                      "d"((uint32_t)(value >> 32)));
}

static struct task *next_task(void) {
    size_t start = current ? (size_t)(current - tasks + 1) : 0;
    for (size_t n = 0; n < TASK_MAX; n++) {
        struct task *task = &tasks[(start + n) % TASK_MAX];
        if (task->runnable) return task;
    }
    return 0;
}

static void switch_to(struct task *next, struct task_frame *frame) {
    if (current) {
        current->frame = *frame;
        __asm__ volatile ("fxsave64 %0" : "=m"(current->fpu));
    }
    current = next;
    vmm_space_activate(&current->space);
    set_fs(current->fs_base);
    __asm__ volatile ("fxrstor64 %0" : : "m"(current->fpu));
    *frame = current->frame;
}

struct task *task_create(void) {
    for (size_t i = 0; i < TASK_MAX; i++) {
        if (tasks[i].pid) continue;
        struct task *task = &tasks[i];
        memset(task, 0, sizeof(*task));
        if (vmm_space_create(&task->space) != 0) return 0;
        task->pid = (int)i + 1;
        task->runnable = 1;
        task->mmap_next = USER_MMAP_BASE;
        task->frame.cs = 0x23;
        task->frame.ss = 0x1b;
        task->frame.rflags = 0x202;
        __asm__ volatile ("fninit; fxsave64 %0" : "=m"(task->fpu));
        return task;
    }
    return 0;
}

struct address_space *task_space(struct task *task) {
    return &task->space;
}

void task_set_entry(struct task *task, uint64_t rip, uint64_t rsp) {
    task->frame.rip = rip;
    task->frame.rsp = rsp;
}

void task_start(void) {
    current = next_task();
    if (!current) goto done;
    vmm_space_activate(&current->space);
    set_fs(current->fs_base);
    __asm__ volatile ("fxrstor64 %0" : : "m"(current->fpu));
    task_enter(&current->frame);
done:
    for (;;) __asm__ volatile ("cli; hlt");
}

void task_yield(struct task_frame *frame) {
    struct task *next = next_task();
    if (next && next != current) switch_to(next, frame);
}

void task_preempt(struct task_frame *frame) {
    task_yield(frame);
}

void task_exit(struct task_frame *frame) {
    struct task *old = current;
    old->runnable = 0;
    struct task *next = next_task();
    if (!next) {
        console_puts("[kernel] userspace exited\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }
    switch_to(next, frame);
    vmm_space_destroy(&old->space);
    memset(old, 0, sizeof(*old));
}

int task_pid(void) {
    return current->pid;
}

uint64_t task_fs_base(void) {
    return current->fs_base;
}

void task_set_fs_base(uint64_t value) {
    current->fs_base = value;
    set_fs(value);
}

uint64_t task_mmap_next(void) {
    return current->mmap_next;
}

void task_set_mmap_next(uint64_t value) {
    current->mmap_next = value;
}

int task_fd_open(const char *path) {
    struct file file;
    if (vfs_open(path, &file) != 0) return -1;
    for (int fd = 3; fd < TASK_FD_MAX; fd++) {
        if (current->fd_used[fd]) continue;
        current->files[fd] = file;
        current->fd_used[fd] = 1;
        return fd;
    }
    return -2;
}

struct file *task_fd_get(int fd) {
    if (fd < 3 || fd >= TASK_FD_MAX || !current->fd_used[fd]) return 0;
    return &current->files[fd];
}

int task_fd_close(int fd) {
    if (!task_fd_get(fd)) return -1;
    current->fd_used[fd] = 0;
    memset(&current->files[fd], 0, sizeof(current->files[fd]));
    return 0;
}
