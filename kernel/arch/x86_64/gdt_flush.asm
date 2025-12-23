[bits 64]
global gdt_flush

section .text
gdt_flush:
    ; The pointer to gdtr is passed in the RDI register (System V ABI)
    lgdt [rdi]            ; Load the new GDT descriptor table

    ; Reload data segment registers
    mov ax, 0x10          ; 0x10 is the offset of Data Segment (gdt[2])
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Reload Code Segment (CS)
    pop  rdi              ; Pop the return address (saved by 'call')
    mov  rax, 0x08        ; 0x08 is the offset of our Code Segment (gdt[1])
    push rax              ; Push new CS onto stack
    push rdi              ; Push return address back onto stack
    retfq                 ; "Return Far" - pops RDI into RIP and RAX into CS