OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x40200000;

    .text : ALIGN(4096)
    {
        *(.text)
        *(.text.*)
        *(.user_code)
        *(.user_code.*)
    } :text

    .rodata : ALIGN(16)
    {
        *(.rodata)
        *(.rodata.*)
        *(.user_rodata)
        *(.user_rodata.*)
    } :text

    .data : ALIGN(4096)
    {
        *(.data)
        *(.data.*)
    } :data

    .bss : ALIGN(4096)
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    } :data

    /DISCARD/ :
    {
        *(.note*)
        *(.comment)
        *(.eh_frame)
        *(.got*)
        *(.igot*)
        *(.plt*)
        *(.rela*)
        *(.rela.*)
        *(.rel*)
        *(.rel.*)
    }
}
