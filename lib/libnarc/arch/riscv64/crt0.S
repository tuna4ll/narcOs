.section .text
.global _start
.type _start, @function
_start:
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    li s0, 0
    call narc_main
    call narc_exit
    ebreak
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
