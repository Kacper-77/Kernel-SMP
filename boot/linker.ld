OUTPUT_FORMAT("elf64-x86-64")
ENTRY(efi_main)
SECTIONS
{
    . = 0x1000; 

    .text : ALIGN(4096)
    {
        *(.text .text.*)
    }

    .rodata : ALIGN(4096)
    {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4096)
    {
        *(.data .data.*)
    }

    .reloc : ALIGN(4096)
    {
        *(.reloc)
    }

    .bss : ALIGN(4096)
    {
        *(.bss .bss.*)
        *(COMMON)
    }
}