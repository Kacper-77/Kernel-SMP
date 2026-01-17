#include <sched.h>
#include <cpu.h>
#include <timer.h>
#include <serial.h>
#include <kmalloc.h>
#include <pmm.h>
#include <vmm.h>
#include <spinlock.h>
#include <std_funcs.h>

task_t* root_task = NULL;
spinlock_t sched_lock_ = { .lock = 0, .owner = -1, .recursion = 0 };

static void idle_task() {
    while (1) {
        __asm__ volatile("hlt");
    }
}

static task_t* create_idle_struct(void (*entry)(void)) {
    task_t* t = kmalloc(sizeof(task_t));
    memset(t, 0, sizeof(task_t));
    
    uint64_t stack_base = (uintptr_t)phys_to_virt((uintptr_t)pmm_alloc_frame());
    uint64_t stack_top = stack_base + 4096;

    interrupt_frame_t* frame = (interrupt_frame_t*)(stack_top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(interrupt_frame_t));

    frame->rip = (uintptr_t)entry;
    frame->cs = 0x08;
    frame->rflags = 0x202;
    frame->rsp = stack_top;
    frame->ss = 0x10;

    t->rsp = (uintptr_t)frame;
    t->tid = 1;
    t->state = TASK_RUNNING;
    t->cpu_id = get_cpu()->cpu_id;
    return t;
}

void sched_init() {
    cpu_context_t* cpu = get_cpu();
    
    task_t* main_task = kmalloc(sizeof(task_t));
    memset(main_task, 0, sizeof(task_t));
    main_task->tid = 0;
    main_task->state = TASK_RUNNING;
    main_task->next = main_task;
    main_task->cpu_id = cpu->cpu_id;
    
    root_task = main_task;
    cpu->current_task = main_task;
    cpu->idle_task = create_idle_struct(idle_task);
    
    cpu->idle_task->next = root_task->next;
    root_task->next = cpu->idle_task;
}

uint64_t schedule(interrupt_frame_t* frame) {
    spin_lock(&sched_lock_);
    
    cpu_context_t* cpu = get_cpu();
    task_t* current = cpu->current_task;
    uint64_t now = get_uptime_ms();

    if (current) {
        current->rsp = (uintptr_t)frame;
        if (current->tid >= 10) {
            current->cpu_id = -1;
        }
    }

    task_t* next = current->next;
    task_t* scheduled_next = NULL;

    for (int i = 0; i < 256; i++) {
        if ((next->tid >= 10 || next->tid == 0) && 
        (next->state == TASK_READY || next->state == TASK_RUNNING) &&
        (next->cpu_id == (uint64_t)-1 || next->cpu_id == cpu->cpu_id) &&
        (now >= next->sleep_until)) {
            
            scheduled_next = next;
            break;
        }
        next = next->next;
        if (next == current->next) break;
    }

    if (!scheduled_next) {
        if (current->tid < 10 && now < current->sleep_until) {
            scheduled_next = cpu->idle_task;
        } else if (current->tid < 10) {
            scheduled_next = current;
        } else {
            scheduled_next = cpu->idle_task;
        }
    }

    if (!scheduled_next) scheduled_next = current;

    scheduled_next->state = TASK_RUNNING;
    scheduled_next->cpu_id = cpu->cpu_id;
    cpu->current_task = scheduled_next;

    spin_unlock(&sched_lock_);
    return scheduled_next->rsp;
}

void sched_init_ap() {
    cpu_context_t* cpu = get_cpu();
    
    cpu->idle_task = create_idle_struct(idle_task); 
    
    spin_lock(&sched_lock_);
    cpu->idle_task->next = root_task->next;
    root_task->next = cpu->idle_task;
    spin_unlock(&sched_lock_);
    
    cpu->current_task = cpu->idle_task;
}

task_t* sched_get_current() {
    return get_cpu()->current_task;
}

void sched_yield() {
    __asm__ volatile("int $32");
}
