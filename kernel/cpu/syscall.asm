[bits 64]
global syscall_entry
extern syscall_handler

;
; Low-level entry point for system calls.
; This handler bridges the gap between the fast 'syscall' instruction
; and the kernel's C-level handler, creating a compatible interrupt frame.
; Currently uses 'iretq' for maximum stability during the SMP development phase.
;

syscall_entry:
    swapgs
    mov [gs:0x18], rsp      ; Save user stack
    mov rsp, [gs:0x20]      ; Load kernel stack

    ; BUILDING FRAME
    push qword 0x23         ; SS 
    push qword [gs:0x18]    ; RSP 
    push r11                ; RFLAGS 
    push qword 0x1B         ; CS 
    push rcx                ; RIP 
    push qword 0            ; error_code
    push qword 0            ; vector_number

    ; PUSH ALL REGS
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp            ; Stack pointer
    call syscall_handler 
    mov rsp, rax            ; Allows to change task

    ; RESTORE ALL REGS
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax

    add rsp, 16             ; Skip vector_number and error_code
          
    swapgs                  ; Restore user GS
    iretq                   ; !!! iretq for now, will be changed !!!
