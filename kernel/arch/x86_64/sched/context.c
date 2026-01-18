#include <sched.h>
#include <kmalloc.h>
#include <spinlock.h>
#include <std_funcs.h>

static uint64_t next_tid = 10;

task_t* arch_task_create(void (*entry_point)(void)) {
    extern task_t* root_task;
    extern spinlock_t sched_lock_;

    task_t* t = kmalloc(sizeof(task_t));
    if (!t) return NULL;
    memset(t, 0, sizeof(task_t));
    
    uint64_t stack_size = 0x4000;
    t->stack_base = (uintptr_t)kmalloc(stack_size);
    if (!t->stack_base) {
        kfree(t);
        return NULL;
    }

    uintptr_t stack_top = (t->stack_base + stack_size) & ~0x0FULL;
    
    // Frame must be below stack_top
    interrupt_frame_t* frame = (interrupt_frame_t*)(stack_top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(interrupt_frame_t));

    frame->rip = (uintptr_t)entry_point;
    frame->cs = 0x08;
    frame->ss = 0x10;
    frame->rflags = 0x202; // IF=1
    
    frame->rsp = (uintptr_t)(stack_top - 8); 
    frame->rbp = (uintptr_t)(stack_top - 8);

    t->rsp = (uintptr_t)frame;
    t->state = TASK_READY;
    t->cpu_id = -1;

    // Lock this TID
    spin_lock(&sched_lock_);
    
    t->tid = next_tid++;
    t->next = root_task->next;
    root_task->next = t;
    
    spin_unlock(&sched_lock_);

    return t;
}
