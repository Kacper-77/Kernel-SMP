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
task_t* dead_task_list = NULL;
spinlock_t sched_lock_ = { .ticket = 0, .current = 0, .last_cpu = -1 };
spinlock_t dead_lock_  = { .ticket = 0, .current = 0, .last_cpu = -1 };

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
             log_flush();
        }
        for(volatile int i=0; i<500; i++) __asm__ volatile("pause");
        __asm__ volatile("hlt");
        sched_yield();  // Trigger scheduler
    }
}

//
// RUNQUEUE SECTION
//
void enqueue_task(cpu_context_t* cpu, task_t* task) {
    spin_lock(&cpu->rq_lock);

    uint8_t p = task->priority;
    if (p >= PRIORITY_LEVELS) p = PRIO_NORMAL;

    task->sched_next = NULL;
    if (cpu->rq_tail[p]) {
        cpu->rq_tail[p]->sched_next = task;
    } else {
        cpu->rq_head[p] = task;
    }
    cpu->rq_tail[p] = task;
    cpu->rq_count[p]++;

    // If task is "homeless", pin it to this CPU for now
    if (task->cpu_id == (uint64_t)-1) {
        task->cpu_id = cpu->cpu_id;
    }

    spin_unlock(&cpu->rq_lock);
}

task_t* dequeue_task(cpu_context_t* cpu) {
    spin_lock(&cpu->rq_lock);

    for (int p = 0; p < PRIORITY_LEVELS - 1; p++) {
        if (cpu->rq_head[p]) {
            task_t* t = cpu->rq_head[p];
            cpu->rq_head[p] = t->sched_next;
            if (!cpu->rq_head[p]) cpu->rq_tail[p] = NULL;
            cpu->rq_count[p]--;

            spin_unlock(&cpu->rq_lock);
            return t;
        }
    }

    spin_unlock(&cpu->rq_lock);
    return NULL;
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
    t->priority   = PRIO_IDLE;
    return t;
}

//
// Attempts to steal a migratable user task (TID >= 10) from another CPU's 
// runqueue to balance load. Respects priority levels and uses trylock 
// to avoid deadlocks during synchronization.
//
static task_t* steal_task_from_cpu(cpu_context_t* other) {
    if (!spin_trylock(&other->rq_lock)) return NULL;

    task_t* stolen = NULL;
    for (int p = 0; p < PRIORITY_LEVELS - 1; p++) {
        task_t* curr = other->rq_head[p];
        task_t* prev = NULL;

        while (curr) {
            if (curr->tid >= 10) {
                if (prev) prev->sched_next = curr->sched_next;
                else other->rq_head[p] = curr->sched_next;

                if (other->rq_tail[p] == curr) other->rq_tail[p] = prev;
                
                other->rq_count[p]--;
                curr->sched_next = NULL;
                stolen = curr;
                break;
            }
            prev = curr;
            curr = curr->sched_next;
        }
        if (stolen) break;
    }

    spin_unlock(&other->rq_lock);
    return stolen;
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
// Core SMP Multi-level Round-Robin Scheduler.
// Handles context switching, load balancing via work stealing, and task states.
// frame is a Pointer to the current interrupt stack frame.
// returns The stack pointer (RSP) of the next task to run.
//
uint64_t schedule(interrupt_frame_t* frame) {
    uint64_t f = spin_irq_save();
    cpu_context_t* cpu = get_cpu();
    task_t* current = cpu->current_task;

    // Wake up sleeping tasks (BSP only)
    if (cpu->cpu_id == 0) sched_update_sleepers();

    if (current) {
        current->rsp = (uintptr_t)frame;
        if (current->state == TASK_RUNNING) {
            current->state = TASK_READY;
            enqueue_task(cpu, current); 
        }

        // Reset affinity for user tasks to allow migration
        if (current->tid >= 10) {
            current->cpu_id = (uint64_t)-1;
        }
    }

    // 1. Fetch task from local runqueue
    task_t* scheduled_next = dequeue_task(cpu);

    // 2. Load Balancing: Attempt to steal from other cores if idle
    if (!scheduled_next) {
        for (int i = 0; i < 32; i++) {
            cpu_context_t* other = get_cpu_by_id(i);
            if (!other || other == cpu) continue;

            scheduled_next = steal_task_from_cpu(other);
            
            if (scheduled_next) {
                break;
            }
        }
    }

    // 3. Final fallback
    if (!scheduled_next) {
        scheduled_next = cpu->idle_task;
    }

    // Update task and CPU state
    scheduled_next->state = TASK_RUNNING;
    scheduled_next->cpu_id = cpu->cpu_id;
    cpu->current_task = scheduled_next;

    // Update TSS/Kernel stack for next interrupt/syscall
    cpu->tss.rsp0 = (uintptr_t)scheduled_next->stack_base + scheduled_next->stack_size;
    cpu->kernel_stack = cpu->tss.rsp0;

    // Address space switch if necessary
    if (scheduled_next->cr3 != 0 && scheduled_next->cr3 != read_cr3()) {
        write_cr3(scheduled_next->cr3);
    }

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
// Gracefully terminates the current task. Marks it as a ZOMBIE and 
// moves it to the 'dead_task_list' for asynchronous cleanup.
// This allows the task to die quickly while the actual memory 
// deallocation is offloaded to the Reaper (CPU 0).
//
void task_exit() {
    // 1. Get current task
    task_t* current = sched_get_current();
    if (!current) return;

    // 2. Lock scheduler
    uint64_t f = spin_irq_save();
    spin_lock(&dead_lock_);

    kprint("\n[SCHED] Task ");
    kprint_hex(current->tid);
    kprint(" is now a ZOMBIE.\n");

    // 3. Add to global "cleanup later" list 
    current->state = TASK_ZOMBIE;
    current->cpu_id = (uint64_t)-1;
    current->sched_next = dead_task_list;
    dead_task_list = current;

    spin_unlock(&dead_lock_);
    spin_irq_restore(f);

    // 4. Trigger immediate reschedule via timer interrupt vector
    while(1) sched_yield(); 
}

//
// The Kernel Reaper: Executed by the Idle task (typically on CPU 0).
// It drains the 'dead_task_list', unlinks tasks from the global 
// 'root_task' list, and physically frees their kernel stacks and structures.
//
void sched_reap() {
    if (!dead_task_list) return;

    // 1. Swap global list with local list
    // And set dead_task_list as NULL
    uint64_t f = spin_irq_save();
    spin_lock(&dead_lock_);

    task_t* to_clean = dead_task_list;
    dead_task_list   = NULL;

    spin_unlock(&dead_lock_);
    spin_irq_restore(f);

    // 2. Iterate through global list and clean it
    // Repeat that until to_clean != NULL
    while (to_clean) {
        task_t* next = to_clean->sched_next;

        spin_lock(&sched_lock_);

        task_t* curr = root_task;
        task_t* prev = NULL;

        // Until current element != element from "to_clean"
        do {
            if (curr->next == to_clean) {
                prev = curr;
                break;
            }
            curr = curr->next;
        } while (curr != root_task);

        if (prev) {
            prev->next = to_clean->next;

            if (to_clean == root_task) {
                root_task = prev;
            }
        }

        spin_unlock(&sched_lock_);

        kprint("[REAPER] Cleaning up TID ");
        kprint_hex(to_clean->tid);
        kprint("\n");

        // 3. Finally free space and update "to_clean" list
        kfree((void*)to_clean->stack_base);
        kfree(to_clean);
        to_clean = next;
    }
}

task_t* sched_get_current() {
    return get_cpu()->current_task;
}

void sched_yield() {
    __asm__ volatile("int $32");
}
