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

void syscall_dispatch(struct syscall_frame *frame);
