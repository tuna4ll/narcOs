.section .text
.global _start
.type _start, @function
_start:
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    li s0, 0
    ld a0, 0(sp)
    addi a1, sp, 8
    slli t0, a0, 3
    add a2, a1, t0
    addi a2, a2, 8
    call __libc_start
    ebreak
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
