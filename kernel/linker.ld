OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(kernel_main)

KERNEL_PHYS = 0x2000000; 
KERNEL_VIRT = 0xFFFFFFFF80000000;

PHDRS
{
    text PT_LOAD FLAGS(5); /* R_E */
    data PT_LOAD FLAGS(6); /* RW_ */
}

SECTIONS {
    . = KERNEL_VIRT;

    .text : AT(KERNEL_PHYS) ALIGN(0x1000) {
        _kernel_start = .;
        _kernel_phys_start = LOADADDR(.text); /* 0x2000000 */
        KEEP(*(.text.entry))
        *(.text*)
    } :text

    .rodata : AT(KERNEL_PHYS + (ADDR(.rodata) - KERNEL_VIRT)) ALIGN(0x1000) {
        *(.rodata*)
    } :text

    .data : AT(KERNEL_PHYS + (ADDR(.data) - KERNEL_VIRT)) ALIGN(0x1000) {
        *(.data*)
    } :data

    .bss : AT(KERNEL_PHYS + (ADDR(.bss) - KERNEL_VIRT)) ALIGN(0x1000) {
        *(COMMON)
        *(.bss*)
        _kernel_end = .;
    } :data

    _kernel_phys_end = KERNEL_PHYS + (_kernel_end - KERNEL_VIRT);
}
