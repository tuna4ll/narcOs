#pragma once
#include <kernel/mm.h>
#include <stdint.h>

struct task_frame {
    uint64_t r15, r14, r13, r12, rbp, rbx;
    uint64_t rax, rdi, rsi, rdx, r10, r8, r9;
    uint64_t rip, cs, rflags, rsp, ss;
};

struct task;

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
