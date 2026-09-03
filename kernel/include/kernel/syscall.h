#pragma once
#include <stdint.h>

struct syscall_frame {
    uint64_t rax;
    uint64_t r9;
    uint64_t r8;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
};

struct fast_syscall_frame {
    uint64_t rax;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t r10;
    uint64_t r8;
    uint64_t r9;
    uint64_t rcx;
    uint64_t r11;
    uint64_t user_rsp;
};

void syscall_init(uint64_t kernel_rsp);
void syscall_dispatch(struct syscall_frame *frame);
void syscall_dispatch_fast(struct fast_syscall_frame *frame);
