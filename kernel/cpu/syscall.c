#include <syscall.h>
#include <serial.h>
#include <sched.h>

void syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    switch (num) {
        case SYS_KPRINT:
            kprint((const char*)arg1); 
            break;

        case SYS_YIELD:
            sched_yield();
            break;

        case SYS_EXIT:
            kprint("[USER] Task requested exit.\n");
            task_exit();
            break;

        default:
            kprint("[SYSCALL] Unknown syscall number: ");
            kprint_hex(num);
            kprint("\n");
            break;
    }
}
