.global _start
.weak _init
.weak _fini
.extern main
.extern __libc_start_main

.section .text
_start:
	xor %rbp,%rbp
	mov (%rsp),%rsi
	lea 8(%rsp),%rdx
	and $-16,%rsp
	lea main(%rip),%rdi
	lea _init(%rip),%rcx
	lea _fini(%rip),%r8
	xor %r9d,%r9d
	call __libc_start_main
	mov %rax,%rbx
	mov $0,%rax
	int $0x80
1:
	int $0x81
	jmp 1b
