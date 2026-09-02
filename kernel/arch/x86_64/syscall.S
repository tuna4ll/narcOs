.section .text
.global syscall_entry
.type syscall_entry, @function
syscall_entry:
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %r8
    pushq %r9
    pushq %rax
    movq %rsp, %rdi
    call syscall_dispatch
    addq $56, %rsp
    iretq
