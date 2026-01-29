[bits 64]
global syscall_entry
extern syscall_handler

syscall_entry:
    swapgs
    mov [gs:0x18], rsp      ; 0x18 = offset user_rsp
    mov rsp, [gs:0x20]      ; 0x20 = offset kernel_stack

    push qword 0x1B     
    push qword [gs:0x18]
    push r11        
    push qword 0x23 
    push rcx    

    push qword 0
    push qword 0

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
    
    pop qword [gs:0x18]     
    add rsp, 8              ; Skip SS
    mov rsp, [gs:0x18]      
    swapgs
    o64 sysret      ;
