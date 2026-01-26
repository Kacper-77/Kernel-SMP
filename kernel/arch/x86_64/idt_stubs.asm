[bits 64]
extern interrupt_dispatch
global isr_stub_table

section .text

%macro isr_no_err_stub 1
isr_stub_%1:
    push qword 0      ; Dummy error code
    push qword %1     ; Vector number
    jmp common_stub
%endmacro

%macro isr_err_stub 1
isr_stub_%1:
    push qword %1     ; Vector number (error code already on stack)
    jmp common_stub
%endmacro

; Define ALL 256 interrupt stubs
%assign i 0
%rep 256
    %if i == 8 || i == 10 || i == 11 || i == 12 || i == 13 || i == 14 || i == 17 || i == 21 || i == 29 || i == 30
        isr_err_stub i
    %else
        isr_no_err_stub i
    %endif
%assign i i+1
%endrep

common_stub:
    ; 1. Save regs
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

    ; 2. GS swap if we go from User Mode
    test qword [rsp + 144], 3
    jz .no_swap_in
    swapgs
.no_swap_in:

    mov rdi, rsp
    
    call interrupt_dispatch
    
    mov rsp, rax               

    ; 3. GS swap if we going back to User Mode
    test qword [rsp + 144], 3
    jz .no_swap_out
    swapgs
.no_swap_out:

    ; 4. Restore state
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

    add rsp, 16                ; Delete vector and error_code
    iretq

section .data
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 256
    dq isr_stub_%+i
%assign i i+1
%endrep