#include <sched.h>
#include <kmalloc.h>
#include <spinlock.h>
#include <std_funcs.h>

static uint64_t next_tid = 10;

task_t* arch_task_create(void (*entry_point)(void)) {
    extern task_t* root_task;
    task_t* t = kmalloc(sizeof(task_t));
    memset(t, 0, sizeof(task_t));
    
    t->tid = next_tid++;
    t->sleep_until = 0;
    t->cpu_id = -1;
    
    uint64_t stack_size = 0x4000;
    t->stack_base = (uintptr_t)kmalloc(stack_size);
    uintptr_t stack_top = t->stack_base + stack_size;

    interrupt_frame_t* frame = (interrupt_frame_t*)(stack_top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(interrupt_frame_t));

    frame->rip = (uintptr_t)entry_point;
    frame->cs = 0x08;
    frame->ss = 0x10;
    frame->rflags = 0x202;
    frame->rsp = (uintptr_t)stack_top;
    frame->rbp = (uintptr_t)stack_top;

    t->rsp = (uintptr_t)frame;
    t->state = TASK_READY;

    extern spinlock_t sched_lock_;
    spin_lock(&sched_lock_);
    t->next = root_task->next;
    root_task->next = t;
    spin_unlock(&sched_lock_);

    return t;
}
