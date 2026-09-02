.section .text
.global enter_userspace
.type enter_userspace, @function
/* rdi = entry, rsi = user stack */
enter_userspace:
    cli
    mov $0x1b, %ax
    mov %ax, %ds
    mov %ax, %es

    pushq $0x1b
    pushq %rsi
    pushfq
    popq %rax
    orq $0x200, %rax
    pushq %rax
    pushq $0x23
    pushq %rdi
    iretq
