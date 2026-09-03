.section .text
.extern exception_die

.macro NOERR name, vec
.global \name
.type \name, @function
\name:
    mov $\vec, %edi
    xor %esi, %esi
    mov 0(%rsp), %rdx
    mov 8(%rsp), %rcx
    andq $-16, %rsp
    call exception_die
1:  cli
    hlt
    jmp 1b
.endm

.macro ERR name, vec
.global \name
.type \name, @function
\name:
    mov $\vec, %edi
    mov 0(%rsp), %rsi
    mov 8(%rsp), %rdx
    mov 16(%rsp), %rcx
    andq $-16, %rsp
    call exception_die
1:  cli
    hlt
    jmp 1b
.endm

NOERR exception_ud, 6
ERR   exception_df, 8
ERR   exception_np, 11
ERR   exception_ss, 12
ERR   exception_gp, 13
ERR   exception_pf, 14
