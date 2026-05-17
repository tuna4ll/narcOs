[BITS 32]
section .user_code
global user_snake_entry_gate
extern user_snake_entry_c

align 4
user_snake_magic:
    dd 0x534E4B33

user_snake_entry_gate:
    push edi
    call user_snake_entry_c
    add esp, 4
.yield_forever:
    int 0x81
    jmp .yield_forever
