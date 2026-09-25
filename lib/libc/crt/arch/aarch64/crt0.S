.section .text
.global _start
.type _start, %function
_start:
    mov x29, xzr
    mov x30, xzr
    ldr x0, [sp]
    add x1, sp, #8
    add x2, x1, x0, lsl #3
    add x2, x2, #8
    bl __libc_start
    brk #0
.size _start, .-_start

.section .note.GNU-stack,"",%progbits
