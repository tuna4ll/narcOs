#pragma once
#include <kernel/arch.h>
#include <kernel/mm.h>
#include <stdint.h>

struct task;
struct file;

struct task *task_create(void);
struct address_space *task_space(struct task *task);
void task_set_entry(struct task *task, uint64_t rip, uint64_t rsp);
void task_start(void) __attribute__((noreturn));
void task_yield(struct task_frame *frame);
void task_preempt(struct task_frame *frame);
void task_exit(struct task_frame *frame);
int task_pid(void);
uint64_t task_fs_base(void);
void task_set_fs_base(uint64_t value);
uint64_t task_mmap_next(void);
void task_set_mmap_next(uint64_t value);
int task_fd_open(const char *path);
struct file *task_fd_get(int fd);
int task_fd_close(int fd);
