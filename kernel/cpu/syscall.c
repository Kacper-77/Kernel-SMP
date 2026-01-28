#include <syscall.h>
#include <serial.h>
#include <cpu.h>
#include <sched.h>

uint64_t syscall_handler(interrupt_frame_t* frame) {
    uint64_t num = frame->rax;
    
    switch (num) {
        case SYS_KPRINT:
            if (frame->rdi < 0xFFFF800000000000) {
                kprint("ERROR!!!!\n");
            }
            kprint((const char*)frame->rdi);
            break;
            
        case SYS_EXIT:
            task_exit();
            break;
    }

    __asm__ volatile("cli");
    return (uint64_t)schedule(frame); 
}
