[bits 16]

section .rodata
global trampoline_start
global trampoline_end

trampoline_start:
[bits 16]
    cli
    cld

    ; 1. Protected Mode
    lgdt [gdt_ptr - trampoline_start + 0x8000]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:(protected_mode - trampoline_start + 0x8000)

[bits 32]
protected_mode:
    mov ax, 0x10
    mov ds, ax
    mov ss, ax

    ; 2. Prepare Long Mode
    mov eax, cr4
    or eax, 1 << 5      ; PAE
    mov cr4, eax

    mov eax, [0x7000 + 8] ; trampoline_pml4
    mov cr3, eax

    mov ecx, 0xC0000080 ; EFER MSR
    rdmsr
    or eax, 1 << 8      ; LME (Long Mode Enable)
    wrmsr

    mov eax, cr0
    or eax, 1 << 31     ; Paging
    mov cr0, eax

    lgdt [gdt64_ptr - trampoline_start + 0x8000]
    jmp 0x18:(long_mode - trampoline_start + 0x8000)

[bits 64]
long_mode:
    mov ax, 0x20
    mov ds, ax
    mov ss, ax

    mov rdi, [0x7000 + 24] ; RDI
    mov rsp, [0x7000 + 0]  ; trampoline_stack
    mov rax, [0x7000 + 16] ; trampoline_entry
    
    mov qword [0x7000 + 32], 1

    jmp rax

align 16
gdt:
    dq 0x0000000000000000 ; Null
    dq 0x00CF9A000000FFFF ; 0x08: Code 32
    dq 0x00CF92000000FFFF ; 0x10: Data 32
    dq 0x00AF9A000000FFFF ; 0x18: Code 64
    dq 0x00AF92000000FFFF ; 0x20: Data 64
gdt_ptr:
    dw $ - gdt - 1
    dd gdt - trampoline_start + 0x8000
gdt64_ptr:
    dw $ - gdt - 1
    dq gdt - trampoline_start + 0x8000

trampoline_end:
