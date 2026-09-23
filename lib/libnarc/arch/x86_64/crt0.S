.section .text
.global _start
.type _start, @function
_start:
    xorq %rbp, %rbp
    andq $-16, %rsp
    call narc_main
    movl %eax, %edi
    call narc_exit
    ud2
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
