#pragma once
#include <kernel/task.h>
#include <stdint.h>

void syscall_init(uint64_t kernel_rsp);
void syscall_dispatch(struct task_frame *frame);
