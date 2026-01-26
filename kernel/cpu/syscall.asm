[bits 64]
global syscall_entry
extern syscall_handler

syscall_entry:
    swapgs                  
    mov [gs:0x10], rsp      
    mov rsp, [gs:0x00]      

    
    push r11                
    push rcx                
    
    
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    

    mov rdi, rsp            
    call syscall_handler

    ; Powrót
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    pop rcx                 
    pop r11                 

    mov rsp, [gs:0x10]      
    swapgs                  
    sysretq