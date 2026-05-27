[BITS 16]
[ORG 0x7C00]

STAGE2_LOAD_SEG equ 0x0000
STAGE2_LOAD_OFF equ 0x7E00

%ifndef STAGE2_SECTORS
%define STAGE2_SECTORS 16
%endif
%ifndef DISK_IMAGE_SECTORS
%define DISK_IMAGE_SECTORS 49152
%endif
%ifndef DISK_BASE_LBA
%define DISK_BASE_LBA 0
%endif
%ifndef PARTITION_START_LBA
%define PARTITION_START_LBA 63
%endif
%ifndef PARTITION_SECTORS
%define PARTITION_SECTORS (DISK_IMAGE_SECTORS - PARTITION_START_LBA)
%endif

BOOT_DISK_BASE_ADDR equ 0x7DF0

start:
    cli

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    sti

    mov [boot_drive], dl
    mov ax, 0x0003
    int 0x10
    mov si, msg_boot
    call print
    call get_drive_geometry

    mov dword [BOOT_DISK_BASE_ADDR], DISK_BASE_LBA
    mov dword [dap_lba_lo], DISK_BASE_LBA + 1
    mov dword [dap_lba_hi], 0
    mov word [dap_count], STAGE2_SECTORS
    mov word [dap_offset], STAGE2_LOAD_OFF
    mov word [dap_segment], STAGE2_LOAD_SEG

    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    pushf
    xor ax, ax
    mov ds, ax
    popf
    jnc .stage2_loaded
    call chs_read_stage2
    jc .disk_err
.stage2_loaded:
    mov si, msg_ok
    call print
    mov dl, [boot_drive]
    jmp STAGE2_LOAD_SEG:STAGE2_LOAD_OFF
.disk_err:
    mov si, msg_err
    call print
    jmp $
print:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print
.done:
    ret
get_drive_geometry:
    push ax
    push cx
    push dx
    mov ah, 0x08
    mov dl, [boot_drive]
    int 0x13
    pushf
    xor ax, ax
    mov ds, ax
    popf
    jc .done
    and cl, 0x3F
    jz .done
    mov [disk_spt], cl
    inc dh
    jz .done
    mov [disk_heads], dh
.done:
    pop dx
    pop cx
    pop ax
    ret
chs_read_stage2:
    push ax
    push bx
    push cx
    push dx
    push es
    xor ax, ax
    mov es, ax
    mov word [chs_lba], DISK_BASE_LBA + 1
    mov word [chs_dst], STAGE2_LOAD_OFF
    mov word [chs_left], STAGE2_SECTORS
.loop:
    mov ax, [chs_lba]
    mov bx, [chs_dst]
    call chs_read_one
    jc .err
    inc word [chs_lba]
    add word [chs_dst], 512
    dec word [chs_left]
    jnz .loop
    clc
    jmp .done
.err:
    stc
.done:
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    ret
chs_read_one:
    push ax
    push bx
    push dx
    mov [chs_saved_bx], bx
    xor dx, dx
    movzx bx, byte [disk_spt]
    div bx
    inc dx
    mov cl, dl
    xor dx, dx
    movzx bx, byte [disk_heads]
    div bx
    cmp ax, 1023
    ja .err
    mov dh, dl
    mov ch, al
    shr ax, 2
    and al, 0xC0
    or cl, al
    mov bx, [chs_saved_bx]
    mov ax, 0x0201
    mov dl, [boot_drive]
    int 0x13
    pushf
    xor ax, ax
    mov ds, ax
    popf
    jc .err
    clc
    jmp .done
.err:
    stc
.done:
    pop dx
    pop bx
    pop ax
    ret

boot_drive  db 0
disk_spt    db 18
disk_heads  db 2
chs_lba     dw 0
chs_dst     dw 0
chs_left    dw 0
chs_saved_bx dw 0

align 4
dap:
    db 0x10, 0x00
dap_count:
    dw STAGE2_SECTORS
dap_offset:
    dw STAGE2_LOAD_OFF
dap_segment:
    dw STAGE2_LOAD_SEG
dap_lba_lo:
    dd 1
dap_lba_hi:
    dd 0

msg_boot    db '[BOOT] NarcOs Stage1 loading...', 0x0D, 0x0A, 0
msg_ok      db '[BOOT] Stage2 loaded. Jumping...', 0x0D, 0x0A, 0
msg_err     db '[ERR]  Disk read failed!', 0x0D, 0x0A, 0

times 446-($-$$) db 0
    db 0x80
    db 0x01, 0x01, 0x00
    db 0x83
    db 0xFE, 0xFF, 0xFF
    dd PARTITION_START_LBA
    dd PARTITION_SECTORS

times 510-($-$$) db 0
dw 0xAA55
