#pragma once
#include <stddef.h>
#include <stdint.h>

#if defined(__x86_64__)
struct task_frame {
    uint64_t r15, r14, r13, r12, rbp, rbx;
    uint64_t rax, rdi, rsi, rdx, r10, r8, r9;
    uint64_t rip, cs, rflags, rsp, ss;
};
#define ARCH_STATE_SIZE 512
#define ARCH_ELF_MACHINE 62
#elif defined(__aarch64__)
struct task_frame {
    uint64_t x[31];
    uint64_t sp, pc, pstate;
};
#define ARCH_STATE_SIZE 528
#define ARCH_ELF_MACHINE 183
#elif defined(__riscv)
struct task_frame {
    uint64_t x[32];
    uint64_t pc, status;
};
#define ARCH_STATE_SIZE 16
#define ARCH_ELF_MACHINE 243
#else
#error Unsupported architecture
#endif

void arch_init(uint64_t kernel_stack);
void arch_halt(void) __attribute__((noreturn));
void arch_set_tls(uint64_t value);
void arch_task_frame_init(struct task_frame *frame);
void arch_task_set_entry(struct task_frame *frame, uint64_t pc, uint64_t sp);
void arch_task_state_init(void *state);
void arch_task_state_save(void *state);
void arch_task_state_restore(const void *state);
uint64_t arch_syscall_number(const struct task_frame *frame);
uint64_t arch_syscall_arg(const struct task_frame *frame, unsigned index);
void arch_syscall_return(struct task_frame *frame, uint64_t value);
