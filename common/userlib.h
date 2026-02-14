#ifndef USERLIB_H
#define USERLIB_H

#include <stdint.h>

// Macros for now cuz compiler is my enemy
#define syscall_3(num, a1, a2, a3) ({ \
    uint64_t _ret; \
    __asm__ volatile ( \
        "syscall" \
        : "=a"(_ret) \
        : "a"((uint64_t)(num)), "D"((uint64_t)(a1)), "S"((uint64_t)(a2)), "d"((uint64_t)(a3)) \
        : "rcx", "r11", "memory" \
    ); \
    _ret; \
})

#define u_print(s)      syscall_3(1, s, 0, 0)
#define u_exit()        syscall_3(2, 0, 0, 0)
#define u_get_uptime()  syscall_3(3, 0, 0, 0)
#define u_sleep(ms)     syscall_3(4, ms, 0, 0)
#define u_read_kbd()    ((char)syscall_3(6, 0, 0, 0))
#define u_print_hex(val)  syscall_3(7, val, 0, 0)
#define u_get_cpuid()   syscall_3(8, 0, 0, 0)

#endif
