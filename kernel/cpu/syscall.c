#include <syscall.h>
#include <serial.h>
#include <cpu.h>
#include <idt.h>
#include <sched.h>
#include <timer.h>

#include <stddef.h>
#include <stdbool.h>

#define KERNEL_SPACE_START 0xFFFF800000000000
#define USER_SPACE_END     0x00007FFFFFFFFFFF

static bool is_user_range(const void* addr, size_t size) {
    uintptr_t start = (uintptr_t)addr;
    uintptr_t end = start + size;

    if (start > USER_SPACE_END || end > USER_SPACE_END || end < start) {
        return false;
    }
    return true;
}

static uint64_t sys_kprint(const char* str) {
    // If pointer is not in lower half
    if ((uintptr_t)str >= KERNEL_SPACE_START) {
        kprint("\n!!!! Pointer is in Higher Half !!!!\n"); 
        return -1;
    }

    // Calculate length
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
        // Additional - String cannot be infinite
        if (len > 4096) return -2; 
    }

    // Verify everything for last time
    if (!is_user_range(str, len + 1)) {
        kprint("\n[SECURITY] Blocked KPRINT: string crosses kernel boundary!\n");
        return -1;
    }

    kprint(str);
    return 0;
}

uint64_t syscall_handler(interrupt_frame_t* frame) {
    uint64_t res = 0;
    uintptr_t sys_num = frame->rax;

    // __asm__ volatile("sti");
    
    switch (sys_num) {
        case SYS_KPRINT:  // RDI: const char* string
            res = sys_kprint((const char*)frame->rdi);
            break;
            
        case SYS_EXIT:
            task_exit();
            break;

        case SYS_GET_UPTIME:
            res = get_uptime_ms();
            break;

        default:
            kprint("Unknown Syscall Number: ");
            kprint_hex(sys_num);
            kprint("\n");
            res = -1;
            break;
    }
    __asm__ volatile("cli");
    frame->rax = res;  // Sys V ABI compatibility

    // Always call scheduler (Preemptive)
    return (uint64_t)schedule(frame); 
}
