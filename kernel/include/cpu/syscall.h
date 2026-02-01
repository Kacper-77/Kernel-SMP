#ifndef SYSCALL_H
#define SYSCALL_H

#include <idt.h>
#include <stdint.h>

typedef uint64_t (*syscall_ptr_t)(interrupt_frame_t*);
extern syscall_ptr_t sys_table[10];

#define SYS_KPRINT 1
#define SYS_EXIT   2
#define SYS_GET_UPTIME 3
#define SYS_SLEEP 4
#define SYS_YIELD  5 // Unused for now

// Global initialization of jump table
void init_sys_table();

#endif
