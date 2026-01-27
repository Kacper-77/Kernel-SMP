#include <syscall.h>
#include <serial.h>
#include <sched.h>

uint64_t syscall_handler(interrupt_frame_t* frame) {
    uint64_t num = frame->rax;
    
    switch (num) {
        case SYS_KPRINT:
            kprint((const char*)frame->rdi); 
            break;
            
        case SYS_EXIT:
            task_exit();
            break;
    }

    return (uint64_t)frame; 
}
