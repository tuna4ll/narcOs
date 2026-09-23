.section .text
.global _start
.type _start, %function
_start:
    mov x29, xzr
    mov x30, xzr
    bl narc_main
    bl narc_exit
    brk #0
.size _start, .-_start

.section .note.GNU-stack,"",%progbits
