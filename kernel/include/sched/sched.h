#ifndef SCHED_H
#define SCHED_H

#include <idt.h>
#include <stdint.h>

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_ZOMBIE,
    TASK_FINISHED
} task_state_t;

typedef struct task {
    uint64_t tid;
    uintptr_t rsp;          
    uintptr_t stack_base;   
    task_state_t state;
    struct task* next;
    uint64_t cpu_id;
    uint64_t sleep_until;
} task_t;

void sched_init();
void sched_init_ap();
uint64_t schedule(interrupt_frame_t* frame);
void sched_yield();
task_t* sched_get_current();

task_t* arch_task_create(void (*entry_point)(void));

#endif
