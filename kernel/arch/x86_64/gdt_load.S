.section .text
.global gdt_load
.type gdt_load, @function
gdt_load:
    lgdt (%rdi)
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    xor %eax, %eax
    mov %ax, %fs
    mov %ax, %gs
    pushq $0x08
    leaq 1f(%rip), %rax
    pushq %rax
    lretq
1:
    ret

.global tss_load
.type tss_load, @function
tss_load:
    mov %di, %ax
    ltr %ax
    ret
