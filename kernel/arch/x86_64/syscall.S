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
    popq %rax
    popq %r9
    popq %r8
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    iretq
.size syscall_entry, .-syscall_entry
