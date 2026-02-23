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
// RUNQUEUE SECTION
//
void enqueue_task(cpu_context_t* cpu, task_t* task) {
    spin_lock(&cpu->rq_lock);

    task->sched_next = NULL;
    if (cpu->rq_tail) {
        cpu->rq_tail->sched_next = task;
    } else {
        cpu->rq_head = task;
    }
    cpu->rq_tail = task;
    cpu->rq_count++;

    spin_unlock(&cpu->rq_lock);
}

task_t* dequeue_task(cpu_context_t* cpu) {
    spin_lock(&cpu->rq_lock);

    task_t* t = cpu->rq_head;
    if (t) {
        cpu->rq_head = t->sched_next;
        if (!cpu->rq_head) cpu->rq_tail = NULL;
        cpu->rq_count--;
    }

    spin_unlock(&cpu->rq_lock);
    return t;
}

static void sched_update_sleepers() {
    uint64_t now = get_uptime_ms();

    spin_lock(&sched_lock_);

    task_t* iter = root_task;
    if (!iter) { spin_unlock(&sched_lock_); return; }

    do {
        if (iter->state == TASK_SLEEPING && now >= iter->sleep_until) {
            iter->state = TASK_READY;
            cpu_context_t* target_cpu = get_cpu_by_id(iter->cpu_id);
            if (!target_cpu) target_cpu = get_cpu(); // Failsafe
            
            enqueue_task(target_cpu, iter);
        }
        iter = iter->next;
    } while (iter != root_task);

    spin_unlock(&sched_lock_);
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

    frame->rip    = (uintptr_t)entry;
    frame->cs     = 0x08;
    frame->rflags = 0x202; 
    frame->rsp    = stack_top - 8;
    frame->ss     = 0x10;

    frame->vector_number = 0;
    frame->error_code = 0;

    t->cr3        = read_cr3(); 
    t->is_user    = false;
    t->stack_size = stack_size;
    t->rsp        = (uintptr_t)frame;
    t->tid        = 1;  // TID 1 reserved for Idle tasks
    t->state      = TASK_RUNNING;
    t->cpu_id     = get_cpu()->cpu_id;
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
// Saves the context of the interrupted task.
// Wakes up tasks that have finished their 'msleep'.
// Selects the next READY task, allowing migration for non-system tasks (TID >= 10).
// Updates TSS for the next interrupt/syscall and performs a CR3 switch if necessary.
//
uint64_t schedule(interrupt_frame_t* frame) {
    // 1. Save IRQ state
    uint64_t f = spin_irq_save();

    cpu_context_t* cpu = get_cpu();
    task_t* current = cpu->current_task;

    // 2. Wake sleepers and enqueue them
    // Only BSP: Work stealing takes care of that
    if (cpu->cpu_id == 0) sched_update_sleepers();

    // Save current task context (per-CPU, no global lock needed)
    if (current) {
    current->rsp = (uintptr_t)frame;

    if (current->state == TASK_RUNNING) {
        current->state = TASK_READY;
        enqueue_task(cpu, current); 
    }

    if (current->tid >= 10)
        current->cpu_id = (uint64_t)-1;
    }

    // 3. Try to get next task from local runqueue
    task_t* scheduled_next = dequeue_task(cpu);

    // 4. If local empty, try to steal from other CPUs
    if (!scheduled_next) {
        for (int i = 0; i < 32; i++) {
            cpu_context_t* other = get_cpu_by_id(i);
            if (!other || other == cpu) continue;

            // try to get a task from that cpu
            spin_lock(&other->rq_lock);
            task_t* t = other->rq_head;
            if (t) {
                other->rq_head = t->sched_next;
                if (!other->rq_head) other->rq_tail = NULL;
                other->rq_count--;
                t->sched_next = NULL;
            }
            spin_unlock(&other->rq_lock);

            if (t) {
                scheduled_next = t;
                break;
            }
        }
    }

    // 5. Fallback to idle
    if (!scheduled_next) scheduled_next = cpu->idle_task;

    // 6. Prepare chosen task
    scheduled_next->state = TASK_RUNNING;
    scheduled_next->cpu_id = cpu->cpu_id;
    cpu->current_task = scheduled_next;

    // 7. Setup kernel stack/TSS and CR3 switch if needed
    cpu->tss.rsp0 = (uintptr_t)scheduled_next->stack_base + scheduled_next->stack_size;
    cpu->kernel_stack = cpu->tss.rsp0;

    if (scheduled_next->cr3 != 0 &&
        scheduled_next->cr3 != read_cr3()) 
            write_cr3(scheduled_next->cr3);

    // 8. restore irq flags and return stack pointer
    spin_irq_restore(f);
    return scheduled_next->rsp;
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
    kprint_hex_raw(current->tid);
    kprint_raw(" is now a ZOMBIE.\n");

    // 3. Change state ("Reaper" can kill it later)
    current->state = TASK_ZOMBIE;
    current->cpu_id = (uint64_t)-1; 

    spin_unlock(&sched_lock_);
    spin_irq_restore(f);

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
    task_t* prev = root_task;
    task_t* current = root_task->next;

    // 3. Go through whole loop 
    while (current != root_task) {
        if (current->state == TASK_ZOMBIE) {
            prev->next = current->next;
            task_t* to_free = current;
            current = current->next;

            kprint_raw("[REAPER] Cleaning up TID ");
            kprint_hex_raw(to_free->tid);
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
