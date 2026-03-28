#ifndef USERLIB_H
#define USERLIB_H

#include <stdint.h>
#include <stddef.h>

/* 
 * General helper for Syscalls
 */
static inline uint64_t syscall_3(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3) {
    uint64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/*
 * USER INTERFACE
 */
static inline void u_print(const char* s) {
    syscall_3(1, (uintptr_t)s, 0, 0);
}

static inline void u_exit(void) {
    syscall_3(2, 0, 0, 0);
}

static inline uint64_t u_get_uptime(void) {
    return syscall_3(3, 0, 0, 0);
}

static inline void u_sleep(uint64_t ms) {
    syscall_3(4, ms, 0, 0);
}

static inline void u_yield(void) {
    syscall_3(5, 0, 0, 0);
}

static inline char u_read_kbd(void) {
    return (char)syscall_3(6, 0, 0, 0);
}

static inline void u_print_hex(uint64_t val) {
    syscall_3(7, val, 0, 0);
}

static inline uint64_t u_get_cpuid(void) {
    return syscall_3(8, 0, 0, 0);
}

static inline uint64_t u_sys_malloc(size_t size) {
    return (uint64_t)syscall_3(9, size, 0, 0);
}

static inline void u_sys_free(uint64_t ptr) {
    syscall_3(10, ptr, 0, 0);
}

static inline uint32_t u_sys_get_tid() {
    return syscall_3(11, 0, 0, 0);
}

static inline uint8_t u_sys_cpu_count() {
    return (uint8_t)syscall_3(12, 0, 0, 0);
}

#endif
