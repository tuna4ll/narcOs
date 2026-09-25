.section .text
.global _start
.type _start, @function
_start:
    xorq %rbp, %rbp
    movq (%rsp), %rdi
    leaq 8(%rsp), %rsi
    leaq 8(%rsi,%rdi,8), %rdx
    andq $-16, %rsp
    call __libc_start
    ud2
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
