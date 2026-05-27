[BITS 16]
[ORG 0x7E00]

KERNEL_OFFSET   equ 0xC000

%ifndef BOOT_MANIFEST_LBA
%define BOOT_MANIFEST_LBA 17
%endif

BOOT_INFO_ADDR  equ 0x7000
BOOT_DISK_BASE_ADDR equ 0x7DF0
DAP_ADDR        equ 0x0600
dap             equ DAP_ADDR
dap_count       equ DAP_ADDR + 2
dap_offset      equ DAP_ADDR + 4
dap_segment     equ DAP_ADDR + 6
dap_lba_lo      equ DAP_ADDR + 8
dap_lba_hi      equ DAP_ADDR + 12
BOOT_MANIFEST_ADDR equ 0x5400
CRC_BUFFER_ADDR equ 0x5600
ELF_HEADER_ADDR equ 0x6800
ELF_HEADER_SECTORS equ 4
DEFER_BUFFER1_ADDR equ 0x9800
DEFER_BUFFER1_SIZE equ 0x2800
DEFER_BUFFER2_ADDR equ 0x4000
DEFER_BUFFER2_SIZE equ 0x1000
DEFER_BUFFER3_ADDR equ 0x5800
DEFER_BUFFER3_SIZE equ 0x0800
DEFER_BUFFER_SIZE equ DEFER_BUFFER1_SIZE + DEFER_BUFFER2_SIZE + DEFER_BUFFER3_SIZE
DEFER_DST_START equ 0x0009C000
DEFER_DST_END   equ 0x000A0000
ELF_PHENTSIZE32 equ 32
ELF_PT_LOAD     equ 1
MAX_ZERO_RANGES equ 4
MAX_LOAD_RANGES equ 8
FILE_BACKED_LIMIT equ 0x0A0000
E820_COUNT_ADDR equ 0x5000
E820_MAP_ADDR   equ 0x5002
E820_MAX_ENTRIES equ 32
BOOT_MANIFEST_MAGIC equ 0x4D43524E
BOOT_MANIFEST_VERSION equ 1
BOOT_MANIFEST_SIZE equ 64
COM1_PORT       equ 0x3F8
BOOT_MAGIC      equ 0x4243524E
BOOT_VERSION    equ 4
BOOT_INFO_SIZE  equ 104
BOOT_FLAG_GRAPHICS equ 0x00000001
BOOT_FLAG_SAFE_TEXT equ 0x00000002
BOOT_FLAG_SERIAL equ 0x00000004
BOOT_FLAG_DEBUG equ 0x00000008
PM16_CODE_SEG equ 0x18
INITRD_LOAD_ADDR equ 0x00A00000
INITRD_MAX_SECTORS equ 32768
INITRD_BUFFER_ADDR equ DEFER_BUFFER2_ADDR
INITRD_BUFFER_SIZE equ DEFER_BUFFER2_SIZE
INITRD_CHUNK_SECTORS equ (INITRD_BUFFER_SIZE / 512)

%ifndef KERNEL_SECTORS
%define KERNEL_SECTORS 128
%endif
%ifndef VBE_WIDTH
%define VBE_WIDTH 1920
%endif
%ifndef VBE_HEIGHT
%define VBE_HEIGHT 1080
%endif

stage2_main:
    mov [boot_drive], dl
    mov eax, [BOOT_DISK_BASE_ADDR]
    mov [disk_base_lba], eax
    call serial_init16
    call get_drive_geometry
    call reset_boot_disk
    mov si, msg_s2
    call print16
    call boot_menu
    mov ax, 0x2401
    int 0x15
    mov si, msg_a20
    call print16
    call detect_memory
    mov si, msg_e820
    call print16
    call find_rsdp
    call load_boot_manifest
    call load_initrd
    test dword [boot_flags], BOOT_FLAG_GRAPHICS
    jz .safe_text
    mov ax, 0x4F00
    mov di, vbe_info_block
    int 0x10
    cmp ax, 0x004F
    jne .vbe_failed

    mov byte [required_bpp], 32
    call find_preferred_mode
    jc .mode_found

    mov byte [required_bpp], 24
    call find_preferred_mode
    jc .mode_found

    mov si, preferred_modes
.find_mode_loop:
    lodsw
    or ax, ax
    jz .vbe_failed        
    mov cx, ax
    or cx, 0x4000        
    mov ax, 0x4F01
    mov di, mode_info_block
    int 0x10
    cmp ax, 0x004F
    jne .find_next
    test word [mode_info_block + 0], 0x0080
    jz .find_next
    cmp byte [mode_info_block + 25], 24
    jb .find_next

    jmp .mode_found

.find_next:
    jmp .find_mode_loop
.mode_found:
    mov [selected_vbe_mode], cx
    push cx
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov si, mode_info_block
    mov di, 0x6100
    mov cx, 128
    rep movsw
    pop cx
    mov si, msg_vbe_ok
    call print16
    mov ax, 0x4F02
    mov bx, cx
    int 0x10
    jmp .after_vbe
.vbe_failed:
    call clear_vbe_handoff
    and dword [boot_flags], 0xFFFFFFFE
    or dword [boot_flags], BOOT_FLAG_SAFE_TEXT
    mov si, msg_vbe_err
    call print16
    jmp .after_vbe
.safe_text:
    call clear_vbe_handoff
    mov si, msg_safe_text
    call print16

.after_vbe:
    call load_kernel
    call flush_deferred_tail
    call write_boot_info
    mov si, msg_gdt
    call print16
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:protected_mode_entry

find_preferred_mode:
    push ax
    push bx
    push dx
    push si
    push di
    push ds
    push es

    mov si, [vbe_info_block + 0x0E]
    mov dx, [vbe_info_block + 0x10]
.scan_loop:
    mov ax, dx
    mov ds, ax
    mov ax, dx
    lodsw
    cmp ax, 0xFFFF
    je .not_found

    mov bx, ax
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov cx, bx
    or cx, 0x4000
    mov ax, 0x4F01
    mov di, mode_info_block
    int 0x10
    cmp ax, 0x004F
    jne .scan_loop
    cmp word [mode_info_block + 18], VBE_WIDTH
    jne .scan_loop
    cmp word [mode_info_block + 20], VBE_HEIGHT
    jne .scan_loop
    test word [mode_info_block + 0], 0x0080
    jz .scan_loop
    mov al, [required_bpp]
    cmp byte [mode_info_block + 25], al
    jne .scan_loop

    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop bx
    pop ax
    stc
    ret

.not_found:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop bx
    pop ax
    clc
    ret

detect_memory:
    pusha
    xor ebx, ebx
    mov di, E820_MAP_ADDR
    push es
    xor ax, ax
    mov es, ax
    xor si, si
.e820_loop:
    cmp si, E820_MAX_ENTRIES
    jae .done
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24
    mov dword [es:di + 20], 1
    int 0x15
    jc .done
    cmp eax, 0x534D4150
    jne .done
    inc si
    add di, 24
    test ebx, ebx
    jnz .e820_loop
.done:
    mov word [E820_COUNT_ADDR], si
    pop es
    popa
    ret

find_rsdp:
    pusha
    push ds
    push es
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov dword [rsdp_addr], 0
    mov ax, [0x040E]
    test ax, ax
    jz .bios_area
    mov cx, 64
    call search_rsdp_segments
    jc .done
.bios_area:
    mov ax, 0xE000
    mov cx, 0x2000
    call search_rsdp_segments
.done:
    pop es
    pop ds
    popa
    ret

search_rsdp_segments:
    push bx
    push cx
.scan:
    test cx, cx
    jz .not_found
    mov ds, ax
    cmp dword [0], 0x20445352
    jne .next
    cmp dword [4], 0x20525450
    jne .next
    mov bx, ax
    call rsdp_checksum20
    jnc .next
    movzx eax, bx
    shl eax, 4
    mov [es:rsdp_addr], eax
    stc
    jmp .out
.next:
    inc ax
    dec cx
    jmp .scan
.not_found:
    clc
.out:
    pop cx
    pop bx
    ret

rsdp_checksum20:
    push ax
    push cx
    push si
    xor ax, ax
    xor si, si
    mov cx, 20
.sum:
    add al, [si]
    inc si
    loop .sum
    test al, al
    pop si
    pop cx
    pop ax
    jnz .bad
    stc
    ret
.bad:
    clc
    ret

boot_menu:
    mov si, msg_menu
    call print16
    mov bx, 60
.wait_key:
    mov ah, 0x01
    int 0x16
    jnz .read_key
    call wait_tick
    dec bx
    jnz .wait_key
    mov si, msg_profile_normal
    call print16
    ret
.read_key:
    xor ah, ah
    int 0x16
    cmp al, '2'
    je .safe
    cmp al, '3'
    je .debug
    mov si, msg_profile_normal
    call print16
    ret
.safe:
    mov byte [boot_profile], 2
    and dword [boot_flags], 0xFFFFFFFE
    or dword [boot_flags], BOOT_FLAG_SAFE_TEXT
    mov si, msg_profile_safe
    call print16
    ret
.debug:
    mov byte [boot_profile], 3
    or dword [boot_flags], BOOT_FLAG_DEBUG
    mov si, msg_profile_debug
    call print16
    ret

wait_tick:
    push ax
    push cx
    push dx
    mov ah, 0x86
    xor cx, cx
    mov dx, 50000
    int 0x15
    jnc .done
    mov cx, 0xFFFF
.delay_loop:
    loop .delay_loop
.done:
    pop dx
    pop cx
    pop ax
    ret

clear_vbe_handoff:
    push ax
    push cx
    push di
    push es
    xor ax, ax
    mov es, ax
    mov di, 0x6100
    xor ax, ax
    mov cx, 128
    rep stosw
    mov di, mode_info_block
    mov cx, 128
    rep stosw
    mov word [selected_vbe_mode], 0
    pop es
    pop di
    pop cx
    pop ax
    ret

write_boot_info:
    push ax
    push di
    push es
    xor ax, ax
    mov es, ax
    mov di, BOOT_INFO_ADDR
    mov dword [es:di + 0], BOOT_MAGIC
    mov word [es:di + 4], BOOT_VERSION
    mov word [es:di + 6], BOOT_INFO_SIZE
    mov eax, [boot_flags]
    mov dword [es:di + 8], eax
    mov al, [boot_drive]
    mov byte [es:di + 12], al
    mov al, [boot_profile]
    mov byte [es:di + 13], al
    mov word [es:di + 14], 0
    mov eax, [kernel_lba]
    mov dword [es:di + 16], eax
    mov eax, [kernel_sector_count]
    mov dword [es:di + 20], eax
    mov ax, [selected_vbe_mode]
    mov word [es:di + 24], ax
    mov word [es:di + 26], VBE_WIDTH
    mov word [es:di + 28], VBE_HEIGHT
    mov ax, [0x5000]
    mov word [es:di + 30], ax
    mov eax, [mode_info_block + 40]
    mov dword [es:di + 32], eax
    movzx eax, word [mode_info_block + 16]
    movzx ecx, word [mode_info_block + 20]
    mul ecx
    mov dword [es:di + 36], eax
    mov eax, [kernel_load_base]
    mov dword [es:di + 40], eax
    mov eax, [kernel_load_end]
    sub eax, [kernel_load_base]
    mov dword [es:di + 44], eax
    mov ax, [mode_info_block + 18]
    mov word [es:di + 48], ax
    mov ax, [mode_info_block + 20]
    mov word [es:di + 50], ax
    mov ax, [mode_info_block + 16]
    mov word [es:di + 52], ax
    mov al, [mode_info_block + 25]
    mov byte [es:di + 54], al
    mov al, [mode_info_block + 27]
    mov byte [es:di + 55], al
    mov ax, [mode_info_block + 31]
    mov word [es:di + 56], ax
    mov ax, [mode_info_block + 33]
    mov word [es:di + 58], ax
    mov ax, [mode_info_block + 35]
    mov word [es:di + 60], ax
    mov ax, [mode_info_block + 37]
    mov word [es:di + 62], ax
    mov dword [es:di + 64], E820_MAP_ADDR
    mov word [es:di + 68], 24
    mov ax, [boot_manifest_version]
    mov word [es:di + 70], ax
    mov eax, [rsdp_addr]
    mov dword [es:di + 72], eax
    mov eax, [kernel_entry]
    mov dword [es:di + 76], eax
    mov eax, [kernel_crc32_expected]
    mov dword [es:di + 80], eax
    mov eax, [initrd_lba]
    mov dword [es:di + 84], eax
    mov eax, [initrd_sector_count]
    mov dword [es:di + 88], eax
    mov eax, [initrd_size]
    mov dword [es:di + 92], eax
    mov eax, [initrd_crc32]
    mov dword [es:di + 96], eax
    mov eax, [initrd_load_addr]
    mov dword [es:di + 100], eax
    pop es
    pop di
    pop ax
    ret
load_kernel:
    mov si, msg_kernel
    call print16

    mov dword [kernel_entry], KERNEL_OFFSET
    mov dword [kernel_load_base], 0xFFFFFFFF
    mov dword [kernel_load_end], 0
    mov byte [zero_range_count], 0
    mov byte [load_range_count], 0
    mov byte [defer_tail_active], 0
    mov dword [defer_tail_end], DEFER_DST_START

    call load_boot_manifest
    ; Full-file CRC touches trailing ELF sectors that are not needed to boot and
    ; can fail on some legacy USB BIOSes. ELF headers and PT_LOAD bounds below
    ; still validate the image before jumping to it.

    mov eax, [kernel_lba]
    mov ebx, ELF_HEADER_ADDR
    mov cx, ELF_HEADER_SECTORS
    call disk_read_sectors

    cmp dword [ELF_HEADER_ADDR + 0], 0x464C457F
    jne .err
    cmp byte [ELF_HEADER_ADDR + 4], 1
    jne .err
    cmp byte [ELF_HEADER_ADDR + 5], 1
    jne .err
    cmp word [ELF_HEADER_ADDR + 16], 2
    jne .err
    cmp word [ELF_HEADER_ADDR + 18], 3
    jne .err
    cmp dword [ELF_HEADER_ADDR + 20], 1
    jne .err
    cmp word [ELF_HEADER_ADDR + 40], 52
    jne .err
    cmp word [ELF_HEADER_ADDR + 42], ELF_PHENTSIZE32
    jne .err
    mov eax, [ELF_HEADER_ADDR + 24]
    mov [kernel_entry], eax

    mov eax, [ELF_HEADER_ADDR + 28]
    mov ebx, eax
    movzx ecx, word [ELF_HEADER_ADDR + 44]
    mov edx, ecx
    imul edx, ELF_PHENTSIZE32
    add edx, ebx
    cmp edx, ELF_HEADER_SECTORS * 512
    ja .err

    mov si, ELF_HEADER_ADDR
    add si, ax
    mov [ph_remaining], cx
.ph_loop:
    cmp word [ph_remaining], 0
    je .done
    cmp dword [si + 0], ELF_PT_LOAD
    jne .next_ph

    mov eax, [si + 12]
    cmp eax, [kernel_load_base]
    jae .base_ok
    mov [kernel_load_base], eax
.base_ok:
    mov eax, [si + 12]
    add eax, [si + 20]
    jc .err
    cmp eax, [kernel_load_end]
    jbe .end_ok
    mov [kernel_load_end], eax
.end_ok:
    mov ebx, [si + 12]
    mov eax, [si + 20]
    call add_load_range
    mov eax, [si + 20]
    cmp eax, [si + 16]
    jb .err
    je .no_zero_range
    sub eax, [si + 16]
    mov ebx, [si + 12]
    add ebx, [si + 16]
    jc .err
    call add_zero_range
.no_zero_range:
    mov eax, [si + 16]
    or eax, eax
    jz .next_ph
    mov edx, [si + 4]
    add edx, eax
    jc .err
    cmp edx, [kernel_file_size]
    ja .err
    mov edx, eax
    add edx, [si + 12]
    jc .err
    cmp edx, FILE_BACKED_LIMIT
    ja .err
    mov eax, [si + 4]
    mov ebx, [si + 12]
    mov ecx, [si + 16]
    push si
    call load_segment_file
    pop si
.next_ph:
    add si, ELF_PHENTSIZE32
    dec word [ph_remaining]
    jmp .ph_loop
.done:
    cmp dword [kernel_load_base], 0xFFFFFFFF
    je .err
    mov si, msg_kernel_ok
    call print16
    ret
.err:
    mov si, msg_kernel_err
    call print16
    jmp $

load_boot_manifest:
    cmp byte [manifest_loaded], 0
    je .read
    ret
.read:
    mov eax, BOOT_MANIFEST_LBA
    mov ebx, BOOT_MANIFEST_ADDR
    mov cx, 1
    call disk_read_sectors
    cmp dword [BOOT_MANIFEST_ADDR + 0], BOOT_MANIFEST_MAGIC
    jne load_kernel.err
    cmp word [BOOT_MANIFEST_ADDR + 4], BOOT_MANIFEST_VERSION
    jne load_kernel.err
    cmp word [BOOT_MANIFEST_ADDR + 6], BOOT_MANIFEST_SIZE
    jb load_kernel.err
    mov ax, [BOOT_MANIFEST_ADDR + 4]
    mov [boot_manifest_version], ax
    mov eax, [BOOT_MANIFEST_ADDR + 12]
    mov [kernel_lba], eax
    mov eax, [BOOT_MANIFEST_ADDR + 16]
    mov [kernel_sector_count], eax
    mov eax, [BOOT_MANIFEST_ADDR + 20]
    mov [kernel_file_size], eax
    mov eax, [BOOT_MANIFEST_ADDR + 24]
    mov [kernel_crc32_expected], eax
    mov eax, [BOOT_MANIFEST_ADDR + 28]
    mov [initrd_lba], eax
    mov eax, [BOOT_MANIFEST_ADDR + 32]
    mov [initrd_sector_count], eax
    mov eax, [BOOT_MANIFEST_ADDR + 36]
    mov [initrd_size], eax
    mov eax, [BOOT_MANIFEST_ADDR + 40]
    mov [initrd_crc32], eax
    cmp dword [kernel_lba], 0
    je load_kernel.err
    cmp dword [kernel_sector_count], 0
    je load_kernel.err
    cmp dword [kernel_sector_count], 0xFFFF
    ja load_kernel.err
    cmp dword [kernel_file_size], 0
    je load_kernel.err
    mov eax, [kernel_sector_count]
    shl eax, 9
    cmp [kernel_file_size], eax
    ja load_kernel.err
    mov byte [manifest_loaded], 1
    ret

load_initrd:
    mov dword [initrd_load_addr], 0
    cmp dword [initrd_sector_count], 0
    je .done
    cmp dword [initrd_lba], 0
    je load_kernel.err
    cmp dword [initrd_sector_count], INITRD_MAX_SECTORS
    ja load_kernel.err

    mov si, msg_initrd
    call print16
    mov eax, [initrd_lba]
    mov [initrd_current_lba], eax
    mov dword [initrd_current_dst], INITRD_LOAD_ADDR
    mov eax, [initrd_sector_count]
    mov [initrd_sectors_left32], eax
.loop:
    cmp dword [initrd_sectors_left32], 0
    je .loaded
    mov eax, [initrd_sectors_left32]
    cmp eax, INITRD_CHUNK_SECTORS
    jbe .chunk_count_ok
    mov eax, INITRD_CHUNK_SECTORS
.chunk_count_ok:
    mov [initrd_chunk_sectors], ax
    mov eax, [initrd_current_lba]
    mov ebx, INITRD_BUFFER_ADDR
    mov cx, [initrd_chunk_sectors]
    call disk_try_read_sectors
    jc .read_failed
    mov esi, INITRD_BUFFER_ADDR
    mov edi, [initrd_current_dst]
    movzx ecx, word [initrd_chunk_sectors]
    shl ecx, 9
    mov [initrd_chunk_bytes], ecx
    call copy_low_to_phys32
    movzx eax, word [initrd_chunk_sectors]
    add [initrd_current_lba], eax
    mov eax, [initrd_chunk_bytes]
    add [initrd_current_dst], eax
    movzx eax, word [initrd_chunk_sectors]
    sub [initrd_sectors_left32], eax
    jmp .loop
.loaded:
    mov dword [initrd_load_addr], INITRD_LOAD_ADDR
.done:
    ret
.read_failed:
    mov ax, [initrd_chunk_sectors]
    sub ax, [sectors_left]
    mov [initrd_chunk_sectors], ax
    jz .count_loaded
    mov esi, DEFER_BUFFER1_ADDR
    mov edi, [initrd_current_dst]
    movzx ecx, ax
    shl ecx, 9
    call copy_low_to_phys32
    movzx eax, word [initrd_chunk_sectors]
    sub [initrd_sectors_left32], eax
.count_loaded:
    mov eax, [initrd_sector_count]
    sub eax, [initrd_sectors_left32]
    jz .none_loaded
    mov [initrd_sector_count], eax
    mov dword [initrd_load_addr], INITRD_LOAD_ADDR
    shl eax, 9
    cmp [initrd_size], eax
    jbe .partial_size_ok
    mov [initrd_size], eax
.partial_size_ok:
    mov si, msg_initrd_partial
    call print16
    jmp .report_error
.none_loaded:
    mov dword [initrd_load_addr], 0
    mov dword [initrd_sector_count], 0
    mov dword [initrd_size], 0
    mov dword [DEFER_BUFFER2_ADDR], 0
    mov esi, DEFER_BUFFER2_ADDR
    mov edi, INITRD_LOAD_ADDR
    mov ecx, 4
    call copy_low_to_phys32
    mov si, msg_initrd_skip
    call print16
.report_error:
    mov si, msg_disk_lba
    call print16
    mov eax, [disk_error_lba]
    call print_hex32
    mov si, msg_disk_ah
    call print16
    mov al, [disk_error_status]
    call print_hex8
    mov si, msg_crlf
    call print16
    ret

copy_low_to_phys32:
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:copy_low_to_phys32_pm

[BITS 32]
copy_low_to_phys32_pm:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    cld
    mov edx, ecx
    shr ecx, 2
    rep movsd
    mov ecx, edx
    and ecx, 3
    rep movsb
    jmp PM16_CODE_SEG:copy_low_to_phys32_pm16

[BITS 16]
copy_low_to_phys32_pm16:
    mov eax, cr0
    and eax, 0xFFFFFFFE
    mov cr0, eax
    jmp 0x0000:copy_low_to_phys32_rm

copy_low_to_phys32_rm:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    sti
    ret

load_segment_file:
    mov [segment_file_offset], eax
    mov [segment_dst], ebx
    mov [segment_bytes_left], ecx
.copy_loop:
    cmp dword [segment_bytes_left], 0
    je .done
    mov eax, [segment_file_offset]
    mov dx, ax
    and dx, 0x01FF
    mov [segment_sector_offset], dx
    shr eax, 9
    add eax, [kernel_lba]
    mov ebx, CRC_BUFFER_ADDR
    mov cx, 1
    call disk_read_sectors
    mov ax, 512
    sub ax, [segment_sector_offset]
    mov [segment_copy_count], ax
    movzx eax, ax
    cmp [segment_bytes_left], eax
    jae .have_count
    mov ax, [segment_bytes_left]
    mov [segment_copy_count], ax
.have_count:
    call clamp_segment_copy_count
    call copy_segment_chunk
    movzx eax, word [segment_copy_count]
    add [segment_file_offset], eax
    add [segment_dst], eax
    sub [segment_bytes_left], eax
    jmp .copy_loop
.done:
    ret

clamp_segment_copy_count:
    push eax
    push edx
    mov eax, [segment_dst]
    movzx edx, word [segment_copy_count]
    test edx, edx
    jz .done
    cmp eax, DEFER_DST_END
    jae .done
    cmp eax, DEFER_DST_START
    jae .deferred
    add edx, eax
    cmp edx, DEFER_DST_START
    jbe .done
    mov eax, DEFER_DST_START
    sub eax, [segment_dst]
    mov [segment_copy_count], ax
    jmp .done
.deferred:
    sub eax, DEFER_DST_START
    cmp eax, DEFER_BUFFER1_SIZE
    jae .second_buffer
    mov edx, DEFER_BUFFER1_SIZE
    sub edx, eax
    movzx eax, word [segment_copy_count]
    cmp eax, edx
    jbe .done
    mov [segment_copy_count], dx
    jmp .done
.second_buffer:
    cmp eax, DEFER_BUFFER1_SIZE + DEFER_BUFFER2_SIZE
    jae .third_buffer
    sub eax, DEFER_BUFFER1_SIZE
    mov edx, DEFER_BUFFER2_SIZE
    sub edx, eax
    movzx eax, word [segment_copy_count]
    cmp eax, edx
    jbe .done
    mov [segment_copy_count], dx
    jmp .done
.third_buffer:
    sub eax, DEFER_BUFFER1_SIZE + DEFER_BUFFER2_SIZE
    cmp eax, DEFER_BUFFER3_SIZE
    jae .overflow
    mov edx, DEFER_BUFFER3_SIZE
    sub edx, eax
    movzx eax, word [segment_copy_count]
    cmp eax, edx
    jbe .done
    mov [segment_copy_count], dx
    jmp .done
.overflow:
    jmp load_kernel.err
.done:
    pop edx
    pop eax
    ret

copy_segment_chunk:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    xor ax, ax
    mov ds, ax
    mov si, CRC_BUFFER_ADDR
    add si, [segment_sector_offset]
    mov eax, [segment_dst]
    cmp eax, DEFER_DST_START
    jb .direct_dest
    cmp eax, DEFER_DST_END
    jae .direct_dest

    mov edx, eax
    sub edx, DEFER_DST_START
    cmp edx, DEFER_BUFFER1_SIZE
    jae .second_defer_buffer
    movzx ebx, word [segment_copy_count]
    add ebx, edx
    cmp ebx, DEFER_BUFFER1_SIZE
    ja .overflow

    mov byte [defer_tail_active], 1
    mov eax, [segment_dst]
    movzx ebx, word [segment_copy_count]
    add eax, ebx
    cmp eax, [defer_tail_end]
    jbe .defer_end_ok
    mov [defer_tail_end], eax
.defer_end_ok:
    mov eax, edx
    add eax, DEFER_BUFFER1_ADDR
    jmp .set_dest

.second_defer_buffer:
    sub edx, DEFER_BUFFER1_SIZE
    cmp edx, DEFER_BUFFER2_SIZE
    jae .third_defer_buffer
    movzx ebx, word [segment_copy_count]
    add ebx, edx
    cmp ebx, DEFER_BUFFER2_SIZE
    ja .overflow

    mov byte [defer_tail_active], 1
    mov eax, [segment_dst]
    movzx ebx, word [segment_copy_count]
    add eax, ebx
    cmp eax, [defer_tail_end]
    jbe .second_defer_end_ok
    mov [defer_tail_end], eax
.second_defer_end_ok:
    mov eax, edx
    add eax, DEFER_BUFFER2_ADDR
    jmp .set_dest

.third_defer_buffer:
    sub edx, DEFER_BUFFER2_SIZE
    movzx ebx, word [segment_copy_count]
    add ebx, edx
    cmp ebx, DEFER_BUFFER3_SIZE
    ja .overflow

    mov byte [defer_tail_active], 1
    mov eax, [segment_dst]
    movzx ebx, word [segment_copy_count]
    add eax, ebx
    cmp eax, [defer_tail_end]
    jbe .third_defer_end_ok
    mov [defer_tail_end], eax
.third_defer_end_ok:
    mov eax, edx
    add eax, DEFER_BUFFER3_ADDR
    jmp .set_dest

.direct_dest:
    mov eax, [segment_dst]
.set_dest:
    mov di, ax
    and di, 0x000F
    shr eax, 4
    mov es, ax
    mov cx, [segment_copy_count]
    cld
    rep movsb
    clc
    jmp .done
.overflow:
    stc
.done:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    jc load_kernel.err
    ret

flush_deferred_tail:
    cmp byte [defer_tail_active], 0
    je .done
    push ax
    push cx
    push si
    push di
    push ds
    push es
    mov eax, [defer_tail_end]
    sub eax, DEFER_DST_START
    cmp eax, DEFER_BUFFER_SIZE
    ja .overflow
    mov [defer_flush_left], ax
    cmp ax, DEFER_BUFFER1_SIZE
    jbe .primary_count_ok
    mov ax, DEFER_BUFFER1_SIZE
.primary_count_ok:
    mov [defer_flush_count], ax
    mov cx, ax
    jcxz .after_primary
    xor ax, ax
    mov ds, ax
    mov si, DEFER_BUFFER1_ADDR
    mov ax, DEFER_DST_START >> 4
    mov es, ax
    xor di, di
    cld
    rep movsb
.after_primary:
    mov ax, [defer_flush_left]
    sub ax, [defer_flush_count]
    jz .out
    cmp ax, DEFER_BUFFER2_SIZE
    jbe .secondary_count_ok
    mov ax, DEFER_BUFFER2_SIZE
.secondary_count_ok:
    mov [defer_flush_count], ax
    mov cx, ax
    xor ax, ax
    mov ds, ax
    mov si, DEFER_BUFFER2_ADDR
    mov ax, (DEFER_DST_START + DEFER_BUFFER1_SIZE) >> 4
    mov es, ax
    xor di, di
    cld
    rep movsb
    mov ax, [defer_flush_left]
    sub ax, DEFER_BUFFER1_SIZE
    sub ax, [defer_flush_count]
    jz .out
    mov cx, ax
    xor ax, ax
    mov ds, ax
    mov si, DEFER_BUFFER3_ADDR
    mov ax, (DEFER_DST_START + DEFER_BUFFER1_SIZE + DEFER_BUFFER2_SIZE) >> 4
    mov es, ax
    xor di, di
    cld
    rep movsb
.out:
    mov byte [defer_tail_active], 0
    clc
    jmp .finish
.overflow:
    stc
.finish:
    pop es
    pop ds
    pop di
    pop si
    pop cx
    pop ax
    jc load_kernel.err
.done:
    ret

add_load_range:
    push cx
    push dx
    push si
    push di
    test eax, eax
    jz .done
    mov edx, ebx
    add edx, eax
    jc load_kernel.err
    movzx cx, byte [load_range_count]
    mov si, load_range_start
    mov di, load_range_end
.check_loop:
    test cx, cx
    jz .store
    cmp edx, [si]
    jbe .next
    cmp ebx, [di]
    jae .next
    jmp load_kernel.err
.next:
    add si, 4
    add di, 4
    dec cx
    jmp .check_loop
.store:
    cmp byte [load_range_count], MAX_LOAD_RANGES
    jae load_kernel.err
    movzx si, byte [load_range_count]
    shl si, 2
    mov [load_range_start + si], ebx
    mov [load_range_end + si], edx
    inc byte [load_range_count]
.done:
    pop di
    pop si
    pop dx
    pop cx
    ret

add_zero_range:
    push di
    cmp byte [zero_range_count], MAX_ZERO_RANGES
    jae load_kernel.err
    movzx di, byte [zero_range_count]
    shl di, 2
    mov [zero_range_start + di], ebx
    mov [zero_range_size + di], eax
    inc byte [zero_range_count]
    pop di
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

reset_boot_disk:
    push ax
    push dx
    xor ax, ax
    mov dl, [boot_drive]
    int 0x13
    xor ax, ax
    mov ds, ax
    pop dx
    pop ax
    ret

disk_read_chs_current:
    push ax
    push bx
    push cx
    push dx
    push es
    mov eax, [dap_lba_lo]
    test eax, 0xFFFF0000
    jnz .err
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
    mov bx, [dap_segment]
    mov es, bx
    mov bx, [dap_offset]
    mov ax, 0x0201
    mov dl, [boot_drive]
    int 0x13
    pushf
    push ax
    xor ax, ax
    mov ds, ax
    pop ax
    mov [disk_error_status], ah
    popf
    jc .err
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

disk_read_sectors:
    add eax, [disk_base_lba]
    jc .err
    mov [dap_lba_lo], eax
    mov dword [dap_lba_hi], 0
    mov dword [disk_error_lba], eax
    mov [read_dst], ebx
    mov [sectors_left], cx
.read_loop:
    mov word [dap + 0], 0x0010
    mov eax, [read_dst]
    mov dx, ax
    and dx, 0x000F
    mov [dap_offset], dx
    shr eax, 4
    mov [dap_segment], ax
    mov word [dap_count], 1
    mov byte [disk_retry], 3
.retry:
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    pushf
    push ax
    xor ax, ax
    mov ds, ax
    pop ax
    mov [disk_error_status], ah
    popf
    jnc .ok
    dec byte [disk_retry]
    jz .try_chs
    mov ah, 0x00
    mov dl, [boot_drive]
    int 0x13
    xor ax, ax
    mov ds, ax
    jmp .retry
.try_chs:
    call disk_read_chs_current
    jc .err
.ok:
    inc dword [dap_lba_lo]
    mov eax, [dap_lba_lo]
    mov [disk_error_lba], eax
    add dword [read_dst], 512
    dec word [sectors_left]
    jnz .read_loop
    ret
.err:
    mov si, msg_disk_err
    call print16
    mov si, msg_disk_lba
    call print16
    mov eax, [disk_error_lba]
    call print_hex32
    mov si, msg_disk_ah
    call print16
    mov al, [disk_error_status]
    call print_hex8
    mov si, msg_crlf
    call print16
    jmp $

disk_try_read_sectors:
    add eax, [disk_base_lba]
    jc .err
    mov [dap_lba_lo], eax
    mov dword [dap_lba_hi], 0
    mov dword [disk_error_lba], eax
    mov [read_dst], ebx
    mov [sectors_left], cx
.read_loop:
    cmp word [sectors_left], 0
    je .done
    mov word [dap + 0], 0x0010
    mov eax, [read_dst]
    mov dx, ax
    and dx, 0x000F
    mov [dap_offset], dx
    shr eax, 4
    mov [dap_segment], ax
    mov word [dap_count], 1
    mov byte [disk_retry], 3
.retry:
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    pushf
    push ax
    xor ax, ax
    mov ds, ax
    pop ax
    mov [disk_error_status], ah
    popf
    jnc .ok
    dec byte [disk_retry]
    jz .try_chs
    mov ah, 0x00
    mov dl, [boot_drive]
    int 0x13
    xor ax, ax
    mov ds, ax
    jmp .retry
.try_chs:
    call disk_read_chs_current
    jc .err
.ok:
    inc dword [dap_lba_lo]
    mov eax, [dap_lba_lo]
    mov [disk_error_lba], eax
    add dword [read_dst], 512
    dec word [sectors_left]
    jmp .read_loop
.done:
    clc
    ret
.err:
    stc
    ret

disk_read_sectors_multi:
    mov [multi_read_lba], eax
    mov [multi_read_dst], ebx
    mov [multi_read_count], cx
    cmp cx, 1
    jbe .fallback
    add eax, [disk_base_lba]
    jc .fallback
    mov [dap_lba_lo], eax
    mov dword [dap_lba_hi], 0
    mov dword [disk_error_lba], eax
    mov word [dap + 0], 0x0010
    mov eax, [multi_read_dst]
    mov dx, ax
    and dx, 0x000F
    mov [dap_offset], dx
    shr eax, 4
    mov [dap_segment], ax
    mov ax, [multi_read_count]
    mov [dap_count], ax
    mov byte [disk_retry], 3
.retry:
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    pushf
    push ax
    xor ax, ax
    mov ds, ax
    pop ax
    mov [disk_error_status], ah
    popf
    jnc .done
    dec byte [disk_retry]
    jz .fallback
    mov ah, 0x00
    mov dl, [boot_drive]
    int 0x13
    xor ax, ax
    mov ds, ax
    jmp .retry
.fallback:
    mov eax, [multi_read_lba]
    mov ebx, [multi_read_dst]
    mov cx, [multi_read_count]
    call disk_read_sectors
.done:
    ret

sectors_left  dw KERNEL_SECTORS
disk_retry    db 3
disk_error_status db 0
disk_spt      db 18
disk_heads    db 2
disk_error_lba dd 0
read_dst      dd 0
multi_read_lba dd 0
multi_read_dst dd 0
multi_read_count dw 0
manifest_loaded db 0
ph_remaining dw 0
boot_manifest_version dw 0
boot_drive    db 0
boot_profile  db 1
selected_vbe_mode dw 0
boot_flags    dd BOOT_FLAG_GRAPHICS | BOOT_FLAG_SERIAL
required_bpp   db 32
disk_base_lba dd 0
kernel_lba dd 18
kernel_sector_count dd KERNEL_SECTORS
kernel_file_size dd 0
kernel_crc32_expected dd 0
kernel_entry dd KERNEL_OFFSET
kernel_load_base dd KERNEL_OFFSET
kernel_load_end dd KERNEL_OFFSET
initrd_lba dd 0
initrd_sector_count dd 0
initrd_size dd 0
initrd_crc32 dd 0
initrd_load_addr dd 0
initrd_current_lba dd 0
initrd_current_dst dd 0
initrd_sectors_left32 dd 0
initrd_chunk_bytes dd 0
initrd_chunk_sectors dw 0
segment_file_offset dd 0
segment_dst dd 0
segment_bytes_left dd 0
segment_sector_offset dw 0
segment_copy_count dw 0
defer_tail_active db 0
defer_tail_end dd DEFER_DST_START
defer_flush_left dw 0
defer_flush_count dw 0
rsdp_addr dd 0
zero_range_count db 0
zero_range_start: times MAX_ZERO_RANGES dd 0
zero_range_size:  times MAX_ZERO_RANGES dd 0
load_range_count db 0
load_range_start: times MAX_LOAD_RANGES dd 0
load_range_end:   times MAX_LOAD_RANGES dd 0
print16:
    lodsb
    or al, al
    jz .done
    push ax
    mov ah, 0x0E
    int 0x10
    pop ax
    call serial_write_char16
    jmp print16
.done:
    ret

print_hex32:
    push eax
    push eax
    shr eax, 24
    call print_hex8
    pop eax
    push eax
    shr eax, 16
    call print_hex8
    pop eax
    push eax
    shr eax, 8
    call print_hex8
    pop eax
    call print_hex8
    pop eax
    ret

print_hex8:
    push ax
    push ax
    shr al, 4
    call print_hex_nibble
    pop ax
    call print_hex_nibble
    pop ax
    ret

print_hex_nibble:
    push ax
    push bx
    and al, 0x0F
    mov bx, hex_digits
    xlatb
    push ax
    mov ah, 0x0E
    int 0x10
    pop ax
    call serial_write_char16
    xor ax, ax
    mov ds, ax
    pop bx
    pop ax
    ret

serial_init16:
    mov dx, COM1_PORT + 1
    xor al, al
    out dx, al
    mov dx, COM1_PORT + 3
    mov al, 0x80
    out dx, al
    mov dx, COM1_PORT + 0
    mov al, 0x03
    out dx, al
    mov dx, COM1_PORT + 1
    xor al, al
    out dx, al
    mov dx, COM1_PORT + 3
    mov al, 0x03
    out dx, al
    mov dx, COM1_PORT + 2
    mov al, 0xC7
    out dx, al
    mov dx, COM1_PORT + 4
    mov al, 0x0B
    out dx, al
    ret

serial_write_char16:
    push ax
    push dx
.wait:
    mov dx, COM1_PORT + 5
    in al, dx
    test al, 0x20
    jz .wait
    pop dx
    pop ax
    mov dx, COM1_PORT
    out dx, al
    ret
align 16
gdt_start:
    dq 0
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 00000000b, 0x00
gdt_end:
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start
msg_s2          db '[S2] Starting Stage 2...', 0x0D, 0x0A, 0
msg_menu        db 0x0D, 0x0A, 'NarcOs Custom Boot', 0x0D, 0x0A
                db '  1. Normal graphics boot', 0x0D, 0x0A
                db '  2. Safe text boot', 0x0D, 0x0A
                db '  3. Serial debug graphics boot', 0x0D, 0x0A
                db 'Select 1-3, or wait for normal...', 0x0D, 0x0A, 0
msg_profile_normal db '[S2] Profile: normal graphics.', 0x0D, 0x0A, 0
msg_profile_safe   db '[S2] Profile: safe text.', 0x0D, 0x0A, 0
msg_profile_debug  db '[S2] Profile: serial debug graphics.', 0x0D, 0x0A, 0
msg_a20         db '[S2] A20 line enabled.', 0x0D, 0x0A, 0
msg_e820        db '[S2] Memory map fetched.', 0x0D, 0x0A, 0
msg_gdt         db '[S2] GDT loading...', 0x0D, 0x0A, 0
msg_vbe_ok      db '[S2] VBE info collected.', 0x0D, 0x0A, 0
msg_vbe_err     db '[ERR] VBE initialization failed!', 0x0D, 0x0A, 0
msg_safe_text   db '[S2] Safe text mode selected; skipping VBE.', 0x0D, 0x0A, 0
msg_kernel      db '[S2] Loading Kernel (LBA)...', 0x0D, 0x0A, 0
msg_kernel_ok   db '[S2] Kernel loaded successfully!', 0x0D, 0x0A, 0
msg_initrd      db '[S2] Payload...', 0x0D, 0x0A, 0
msg_initrd_partial db '[WARN] Payload partial; keeping loaded prefix.', 0x0D, 0x0A, 0
msg_initrd_skip db '[WARN] Payload read skipped.', 0x0D, 0x0A, 0
msg_kernel_err  db '[ERR] Kernel load failed!', 0x0D, 0x0A, 0
msg_disk_err    db '[ERR] Disk read failed!', 0x0D, 0x0A, 0
msg_disk_lba    db '[ERR] LBA=0x', 0
msg_disk_ah     db ' AH=0x', 0
msg_crlf        db 0x0D, 0x0A, 0
hex_digits      db '0123456789ABCDEF'
preferred_modes: dw 0x011B, 0x0143, 0x0118, 0x0115, 0x0112, 0 
vbe_info_block:  times 512 db 0
mode_info_block: times 256 db 0
[BITS 32]
protected_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov esp, 0x90000
    call zero_elf_ranges32
    mov eax, [kernel_entry]
    jmp eax

zero_elf_ranges32:
    movzx edx, byte [zero_range_count]
    mov esi, zero_range_start
    mov ebx, zero_range_size
.range_loop:
    test edx, edx
    jz .done
    mov edi, [esi]
    mov ecx, [ebx]
    xor eax, eax
    rep stosb
    add esi, 4
    add ebx, 4
    dec edx
    jmp .range_loop
.done:
    ret
