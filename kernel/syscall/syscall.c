#include <syscall.h>
#include <serial.h>
#include <ps2_kbd.h>

#include <stddef.h>

/*
 * Handle syscalls based on sys_table (jump table),
 * all syscalls are wrapped in syscall_funcs.c
 */
uint64_t syscall_handler(interrupt_frame_t* frame) {
    // Fetch syscall from RAX
    uintptr_t sys_num = frame->rax;
    
    if (sys_table[sys_num] == NULL) {
        kprintf("[SYSCALL] Unknown or unimplemented: %p\n", (uint64_t)sys_num);
        frame->rax = -1;
        return (uintptr_t)frame;
    }

    uint64_t res = sys_table[sys_num](frame);

    // Special case, if syscall returns schedule(frame)
    if (sys_num == SYS_SLEEP || 
        sys_num == SYS_EXIT) 
        return res;

    // Sys V ABI compatibility
    frame->rax = res;

    return (uintptr_t)frame; 
}
