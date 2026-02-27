#ifndef SCHED_H
#define SCHED_H

#include <idt.h>
#include <cpu.h>

#include <stdint.h>
#include <stdbool.h>

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
    uint64_t stack_size;
    task_state_t state;
    bool is_user;       
    uintptr_t cr3;

    struct task* next;        // Global list for Reaper
    struct task* sched_next;  // Runqueue Per-CPU
    uint8_t priority;

    uint64_t cpu_id;
    uint64_t sleep_until;
} __attribute__((aligned(64))) task_t;

void sched_init();
void sched_init_ap();
uint64_t schedule(interrupt_frame_t* frame);
void sched_yield();
void task_exit();
void sched_reap();

void enqueue_task(cpu_context_t* cpu, task_t* task);
task_t* dequeue_task(cpu_context_t* cpu);

task_t* sched_get_current();
task_t* arch_task_create(void (*entry_point)(void));
task_t* arch_task_create_user(void (*entry_point)(void));
task_t* arch_task_spawn_elf(void* elf_raw_data);

#endif
