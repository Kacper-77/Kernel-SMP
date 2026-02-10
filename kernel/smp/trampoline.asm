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
    mov es, ax
    mov ss, ax
    xor ax, ax          
    mov fs, ax          ; FS = 0
    mov gs, ax          ; GS = 0 

    mov rdi, [0x7000 + 24] ; cpu_context_p
    mov rsp, [0x7000 + 0]  ; trampoline_stack
    
    ; ALLIGN STACK
    and rsp, -16               ; allign RSP
    sub rsp, 8            
                          
    mov rax, [0x7000 + 16]     ; trampoline_entry (kernel_main_ap)
    mov qword [0x7000 + 32], 1 ; trampoline_ready

    jmp rax

align 16
gdt:
    dq 0x0000000000000000 ; Null
    dq 0x00CF9A000000FFFF ; 0x08: Code 32
    dq 0x00CF92000000FFFF ; 0x10: Data 32
    dq 0x00AF9A000000FFFF ; 0x18: Code 64
    dq 0x00AF92000000FFFF ; 0x20: Data 64
    dq 0x00AFFB000000FFFF ; 0x28: Code 64 (User)  <- DPL=3, Readable, Executable
    dq 0x00AFF3000000FFFF ; 0x30: Data 64 (User)
gdt_ptr:
    dw $ - gdt - 1
    dd gdt - trampoline_start + 0x8000
gdt64_ptr:
    dw $ - gdt - 1
    dq gdt - trampoline_start + 0x8000

trampoline_end:
