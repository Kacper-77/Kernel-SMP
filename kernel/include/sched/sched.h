#ifndef SCHED_H
#define SCHED_H

#include <idt.h>
#include <cpu.h>
#include <atomic.h>
#include <vma.h>
#include <sched_utils.h>

#include <stdint.h>
#include <stdbool.h>

typedef struct task {
    uint64_t  tid;
    uintptr_t rsp;          
    uintptr_t stack_base;   
    uint64_t  stack_size;

    task_state_t  state;
    task_reason_t wait_reason;
    bool is_user;       
    uintptr_t cr3;

    // VMA & Memory Management
    struct vma_area* vma_tree_root; 
    struct vma_area* vma_list_head;
    mutex_t    vma_mutex;
    uint64_t   vma_count; 
    
    uintptr_t heap_start;
    uintptr_t heap_curr;
    uintptr_t heap_end;          

    struct task* next;        // Global list for Reaper
    struct task* prev;  
    struct task* sched_next;  // Runqueue Per-CPU
    task_prio_t  priority;
    task_prio_t  base_priority;

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
void sched_update_sleepers();
void sched_make_task_sleep(uint64_t ms);
void sched_block_current(task_reason_t reason);
void sched_wakeup(task_reason_t reason);

task_t* sched_get_current();
task_t* arch_task_create(void (*entry_point)(void));
task_t* arch_task_create_user(void (*entry_point)(void));
task_t* arch_task_spawn_elf(void* elf_raw_data);

#endif
