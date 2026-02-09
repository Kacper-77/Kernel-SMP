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
spinlock_t sched_lock_ = { .ticket = 0, .current = 0, .last_cpu = -1 };

//
// The Idle Task: The ultimate fallback for the CPU when no tasks are ready.
// It keeps the processor in a low-power state (HLT). On CPU 0, this task 
// also acts as the 'Reaper', responsible for deallocating ZOMBIE tasks 
// to prevent memory leaks in the scheduler.
//
static void idle_task() {
    while (1) {
        if (get_cpu()->cpu_id == 0) {
             sched_reap();
        }  
        __asm__ volatile("hlt");
        sched_yield();  // Trigger scheduler
    }
}

//
// Allocates and initializes the idle task structure for a specific CPU.
//
static task_t* create_idle_struct(void (*entry)(void)) {
    task_t* t = kmalloc(sizeof(task_t));
    memset(t, 0, sizeof(task_t));
    
    uint64_t stack_size = 4096;
    t->stack_base = (uintptr_t)kmalloc(stack_size); 
    uint64_t stack_top = t->stack_base + stack_size;
    stack_top &= ~0xFULL;  // Ensure 16-byte alignment

    // Setup the interrupt frame to simulate an interrupt return to the entry point
    interrupt_frame_t* frame = (interrupt_frame_t*)(stack_top - sizeof(interrupt_frame_t) - 8);
    memset(frame, 0, sizeof(interrupt_frame_t));

    frame->rip = (uintptr_t)entry;
    frame->cs = 0x08;
    frame->rflags = 0x202; 
    frame->rsp = stack_top - 8;
    frame->ss = 0x10;

    frame->vector_number = 0;
    frame->error_code = 0;

    t->cr3 = read_cr3(); 
    t->is_user = false;
    t->stack_size = stack_size;
    t->rsp = (uintptr_t)frame;
    t->tid = 1;  // TID 1 reserved for Idle tasks
    t->state = TASK_RUNNING;
    t->cpu_id = get_cpu()->cpu_id;
    return t;
}

//
// Initializes the scheduler on the Bootstrap Processor (BSP).
//
void sched_init() {
    cpu_context_t* cpu = get_cpu();
    
    // Convert current execution flow into the 'Main' task (TID 0)
    task_t* main_task = kmalloc(sizeof(task_t));
    memset(main_task, 0, sizeof(task_t));
    main_task->tid = 0;
    main_task->state = TASK_RUNNING;
    main_task->next = main_task;
    main_task->cpu_id = cpu->cpu_id;
    main_task->is_user = false;
    main_task->cr3 = read_cr3();
    main_task->stack_size = 0;
    
    root_task = main_task;
    cpu->current_task = main_task;
    cpu->idle_task = create_idle_struct(idle_task);
}

//
// The core Round-Robin scheduler with support for task sleeping and CPU affinity.
// 1. Saves the context of the interrupted task.
// 2. Wakes up tasks that have finished their 'msleep'.
// 3. Selects the next READY task, allowing migration for non-system tasks (TID >= 10).
// 4. Updates TSS for the next interrupt/syscall and performs a CR3 switch if necessary.
//
uint64_t schedule(interrupt_frame_t* frame) {
    spin_lock(&sched_lock_);
    
    cpu_context_t* cpu = get_cpu();
    task_t* current = cpu->current_task;
    uint64_t now = get_uptime_ms();

    // Save current task context
    if (current) {
        current->rsp = (uintptr_t)frame;
        if (current->state == TASK_RUNNING) current->state = TASK_READY;
        // Allow migration for generic tasks (TID >= 10)
        if (current->tid >= 10) current->cpu_id = (uint64_t)-1;
    }

    task_t* scheduled_next = NULL;
    task_t* iter = root_task;

    // Search for the next available task
    for (int i = 0; i < 256; i++) {
        // Wake up sleeping tasks if time has passed
        if (iter->state == TASK_SLEEPING) {
            if (now >= iter->sleep_until) {
                iter->state = TASK_READY;
            }
        }

        // Pick next READY task (considering CPU affinity)
        if (scheduled_next == NULL) {
            if (iter->state == TASK_READY && (iter->cpu_id == (uint64_t)-1 || iter->cpu_id == cpu->cpu_id)) {
                iter->state = TASK_RUNNING; 
                iter->cpu_id = cpu->cpu_id;
                scheduled_next = iter;
            }
        }

        iter = iter->next;
        if (iter == root_task) break;
    }

    // Fallback to CPU-specific idle task if no tasks are ready
    if (!scheduled_next) {
        scheduled_next = cpu->idle_task;
    }

    scheduled_next->state = TASK_RUNNING;
    scheduled_next->cpu_id = cpu->cpu_id;
    cpu->current_task = scheduled_next;

    cpu->current_task = scheduled_next;

    // Set stack for syscall/interrupt
    cpu->tss.rsp0 = (uintptr_t)scheduled_next->stack_base + scheduled_next->stack_size;
    cpu->kernel_stack = cpu->tss.rsp0;

    if (scheduled_next->cr3 != 0) {
        uint64_t current_cr3 = read_cr3();
        if (scheduled_next->cr3 != current_cr3) {
            write_cr3(scheduled_next->cr3);
        }
    }

    spin_unlock(&sched_lock_);
    return scheduled_next->rsp;  // Return stack pointer for context switch
}

//
// Bootstraps the scheduler on an Application Processor (AP).
// Creates a CPU-specific idle context and sets it as the starting task,
// allowing the AP to begin accepting work from the global task list.
//
void sched_init_ap() {
    cpu_context_t* cpu = get_cpu();
    
    cpu->idle_task = create_idle_struct(idle_task); 
    cpu->current_task = cpu->idle_task;
}

//
// Gracefully terminates the current task. Marks the task as a ZOMBIE,
// allowing its memory to be safely reclaimed by the Reaper on a different
// execution context. Triggers an immediate reschedule.
//
void task_exit() {
    // 1. Get current task
    task_t* current = sched_get_current();
    if (!current) return;

    // 2. Lock scheduler
    uint64_t f = spin_irq_save();
    spin_lock(&sched_lock_);

    kprint_raw("\n[SCHED] Task ");
    kprint_hex(current->tid);
    kprint_raw(" is now a ZOMBIE.\n");

    // 3. Change state ("Reaper" can kill it later)
    current->state = TASK_ZOMBIE;
    current->cpu_id = (uint64_t)-1; 

    spin_unlock(&sched_lock_);

    // 4. Trigger immediate reschedule via timer interrupt vector
    while(1) {
        sched_yield(); 
        __asm__ volatile("hlt");
    }
}

//
// The Kernel Reaper: Scans the global task list to physically free memory
// associated with terminated (ZOMBIE) tasks. This ensures that kernel stacks 
// and task structures are recycled.
//
void sched_reap() {
    // 1. Lock before cleaning
    uint64_t f = spin_irq_save();
    spin_lock(&sched_lock_);

    // 2. Extract key data about tasks
    extern task_t* root_task;
    task_t* prev = root_task;
    task_t* current = root_task->next;

    // 3. Go through whole loop 
    while (current != root_task) {
        if (current->state == TASK_ZOMBIE) {
            prev->next = current->next;
            task_t* to_free = current;
            current = current->next;

            kprint_raw("[REAPER] Cleaning up TID ");
            kprint_hex(to_free->tid);
            kprint_raw("\n");

            kfree((void*)to_free->stack_base); 
            kfree(to_free);                    
            
            continue;
        }
        
        prev = current;
        current = current->next;
    }
    spin_unlock(&sched_lock_);
    spin_irq_restore(f);
}

task_t* sched_get_current() {
    return get_cpu()->current_task;
}

void sched_yield() {
    __asm__ volatile("int $32");
}
