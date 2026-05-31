.text
.global __set_thread_area
.hidden __set_thread_area
.type __set_thread_area,@function
__set_thread_area:
	push %rbx
	mov %rdi,%rbx
	mov $90,%eax
	int $0x80
	pop %rbx
	ret
