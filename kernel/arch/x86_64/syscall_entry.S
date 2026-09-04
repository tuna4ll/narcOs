.section .text
.global syscall_entry
.type syscall_entry, @function
.extern syscall_kernel_rsp
.extern syscall_user_rsp
.extern syscall_dispatch

syscall_entry:
    /* SYSCALL does not switch stacks on x86_64. Save the userspace RSP
       before touching the kernel stack. This template is deliberately
       single-core/single-process, so one scratch slot is sufficient. */
    movq %rsp, syscall_user_rsp(%rip)
    movq syscall_kernel_rsp(%rip), %rsp

    /* Keep this layout in sync with struct syscall_frame. Ten pushes
       also leave the stack correctly aligned for the SysV C call below. */
    pushq syscall_user_rsp(%rip)
    pushq %r11
    pushq %rcx
    pushq %r9
    pushq %r8
    pushq %r10
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rax

    movq %rsp, %rdi
    call syscall_dispatch

    /* The C dispatcher is allowed to clobber all SysV caller-saved
       registers, but userspace syscall wrappers only expect RAX plus the
       architectural RCX/R11 clobbers. Restore the argument registers. */
    movq 0(%rsp), %rax
    movq 8(%rsp), %rdi
    movq 16(%rsp), %rsi
    movq 24(%rsp), %rdx
    movq 32(%rsp), %r10
    movq 40(%rsp), %r8
    movq 48(%rsp), %r9
    movq 56(%rsp), %rcx
    movq 64(%rsp), %r11
    movq 72(%rsp), %rsp
    sysretq
.size syscall_entry, .-syscall_entry
