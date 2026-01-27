[bits 64]
global syscall_entry
extern syscall_handler

syscall_entry:
    swapgs
    mov [gs:0x10], rsp
    mov rsp, [gs:0x00]

    push qword 0x1B     
    push qword [gs:0x10]
    push r11        
    push qword 0x23 
    push rcx    

    push qword 0
    push qword 0

    push rax                ; r_rax
    push rcx                ; r_rcx
    push rdx                ; r_rdx
    push rbx                ; r_rbx
    push rbp                ; r_rbp
    push rsi                ; r_rsi
    push rdi                ; r_rdi
    push r8                 ; r_r8
    push r9                 ; r_r9
    push r10                ; r_r10
    push r11                ; r_r11
    push r12                ; r_r12
    push r13                ; r_r13
    push r14                ; r_r14
    push r15                ; r_r15

   
    mov rdi, rsp            
    call syscall_handler 

    mov rsp, rax

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

    add rsp, 16

    pop rcx  
    add rsp, 8
    pop r11   
    
    pop qword [gs:0x10] 
    add rsp, 8          

    mov rsp, [gs:0x10]
    swapgs
    sysretq
