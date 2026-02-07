#ifndef USERLIB_H
#define USERLIB_H

#include <stdint.h>

static inline uint64_t do_syscall(uint64_t num, uint64_t arg1) {
    uint64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline void u_print(const char* s) { do_syscall(1, (uintptr_t)s); }
static inline void u_exit() { do_syscall(2, 0); }
static inline uint64_t u_get_uptime() { return do_syscall(3, 0); }
static inline void u_sleep(uint64_t ms) { do_syscall(4, ms); }
static inline char u_read_kbd() { 
    return (char)do_syscall(6, 0); 
}

#endif
