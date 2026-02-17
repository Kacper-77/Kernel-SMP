#include <syscall.h>
#include <sched.h>
#include <timer.h>
#include <serial.h>
#include <ps2_kbd.h>
#include <cpu.h>

#include <stdint.h>
#include <stddef.h>

#define KERNEL_SPACE_START 0xFFFF800000000000
#define USER_SPACE_END     0x00007FFFFFFFFFFF

//
// HELPERS
//
static bool is_user_range(const void* addr, size_t size) {
    uintptr_t start = (uintptr_t)addr;
    uintptr_t end = start + size;

    if (start > USER_SPACE_END || end > USER_SPACE_END || end < start) {
        return false;
    }
    return true;
}

static uint64_t sys_kprint(const char* str) {
    if (!str) return -1;

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

static uint64_t sys_kprint_hex(uint64_t value) {
    kprint_hex(value);
    return 0;
}

//
// FUNCS WRAPPERS
//
uint64_t sys_kprint_handler(interrupt_frame_t* frame) {
    return sys_kprint((const char*)frame->rdi);
}

uint64_t sys_kprint_hex_handler(interrupt_frame_t* frame) {
    return sys_kprint_hex((uint64_t)frame->rdi);
}

uint64_t sys_get_cpuid_handler(interrupt_frame_t* frame) {
    (void)frame;
    return get_cpu()->cpu_id;
}

uint64_t sys_exit_handler(interrupt_frame_t* frame) {
    (void)frame;
    task_exit();

    return (uint64_t)schedule(frame);
}

uint64_t sys_get_uptime_handler(interrupt_frame_t* frame) {
    (void)frame;
    return get_uptime_ms();
}

uint64_t sys_sleep_handler(interrupt_frame_t* frame) {
    task_t* current = sched_get_current();
    // RDI holds ms
    current->sleep_until = get_uptime_ms() + frame->rdi;
    current->state = TASK_SLEEPING;

    return (uint64_t)schedule(frame);
}

uint64_t sys_read_kbd_handler(interrupt_frame_t* frame) {
    (void)frame;
    return (uint64_t)kbd_pop_char();
}

//
// INIT SYS TABLE
//
void init_sys_table() {
    sys_table[SYS_KPRINT]     = sys_kprint_handler;
    sys_table[SYS_EXIT]       = sys_exit_handler;
    sys_table[SYS_GET_UPTIME] = sys_get_uptime_handler;
    sys_table[SYS_SLEEP]      = sys_sleep_handler;
    sys_table[SYS_KBD_PS2]    = sys_read_kbd_handler;
    sys_table[SYS_KPRINT_HEX] = sys_kprint_hex_handler;
    sys_table[SYS_CPU_ID]     = sys_get_cpuid_handler;
}
