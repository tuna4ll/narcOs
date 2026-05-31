.global _start
.weak _init
.weak _fini
.extern main
.extern __libc_start_main

.section .text
_start:
	xor %ebp,%ebp
	mov (%esp),%eax
	lea 4(%esp),%ebx
	push $0
	push $_fini
	push $_init
	push %ebx
	push %eax
	push $main
	call __libc_start_main
	mov %eax,%ebx
	mov $0,%eax
	int $0x80
1:
	int $0x81
	jmp 1b
