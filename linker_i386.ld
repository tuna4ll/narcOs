/* linker_i386.ld */
OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

SECTIONS
{
    . = 0xC000;

    .text : ALIGN(4096)
    {
        *(.text.prologue)
        . = ALIGN(4096);
        *(.text)
        *(.text.*)
    }

    .user_region : ALIGN(4096)
    {
        __user_region_start = .;
        *(.user_code)
        *(.user_code.*)
        *(.user_rodata)
        *(.user_rodata.*)
        obj/i386/apps/user_shell.o(.rodata)
        obj/i386/apps/user_shell.o(.rodata.*)
        obj/i386/apps/user_shell.o(.data)
        obj/i386/apps/user_shell.o(.data.*)
        obj/i386/apps/user_shell.o(.bss)
        obj/i386/apps/user_shell.o(.bss.*)
        obj/i386/apps/user_net_apps.o(.rodata)
        obj/i386/apps/user_net_apps.o(.rodata.*)
        obj/i386/apps/user_net_apps.o(.data)
        obj/i386/apps/user_net_apps.o(.data.*)
        obj/i386/apps/user_net_apps.o(.bss)
        obj/i386/apps/user_net_apps.o(.bss.*)
        . = ALIGN(4096);
        __user_region_end = .;
    }

    .rodata : ALIGN(4)
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data : ALIGN(4)
    {
        *(.data)
        *(.data.*)
    }

    ASSERT(. <= 0x000A0000, "Kernel file-backed image overlaps VGA memory aperture")

    . = 0x00100000;

    .bss : ALIGN(4)
    {
        __bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss*)
        __bss_end = .;
        __kernel_end = .;
    }

    /DISCARD/ :
    {
        *(.note*)
        *(.indent)
        *(.comment)
        *(.eh_frame)
        *(.rel.plt)
    }
}
