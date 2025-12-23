[bits 64]
extern exception_handler
global isr_stub_table

section .text

%macro isr_no_err_stub 1
isr_stub_%1:
    push 0              ; Dummy error code
    push %1             ; Vector number
    jmp common_stub
%endmacro

%macro isr_err_stub 1
isr_stub_%1:
    push %1             ; Vector number (error code is already pushed by CPU)
    jmp common_stub
%endmacro

; Define all 32 exceptions
%assign i 0
%rep 32
    ; These exceptions push their own error code
    %if i == 8 || i == 10 || i == 11 || i == 12 || i == 13 || i == 14 || i == 17 || i == 21 || i == 29 || i == 30
        isr_err_stub i
    %else
        isr_no_err_stub i
    %endif
%assign i i+1
%endrep

common_stub:
    ; SAVE STATE: Push all general purpose registers
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

    ; Pass pointer to the stack (the registers structure) to C
    mov rdi, rsp
    
    ; Calling C handler
    call exception_handler

    ; RESTORE STATE
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

    ; Cleanup vector number and error code
    add rsp, 16
    iretq

section .data
isr_stub_table:
%assign i 0
%rep 32
    dq isr_stub_%+i
%assign i i+1
%endrep