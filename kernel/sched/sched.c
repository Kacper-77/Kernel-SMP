#include <sched.h>
#include <cpu.h>
#include <timer.h>
#include <serial.h>
#include <kmalloc.h>
#include <pmm.h>
#include <vmm.h>
#include <vma.h>
#include <spinlock.h>
#include <std_funcs.h>

task_t* root_task          = NULL;
task_t* dead_task_list     = NULL;
task_t* blocked_task_list  = NULL;

spinlock_t sched_lock_    = { .ticket = 0, .current = 0, .last_cpu = -1 };
spinlock_t dead_lock_     = { .ticket = 0, .current = 0, .last_cpu = -1 };  // task_exit, sched_reap
spinlock_t sleep_lock_    = { .ticket = 0, .current = 0, .last_cpu = -1 };  // sched_update_sleepers
spinlock_t blocked_lock_  = { .ticket = 0, .current = 0, .last_cpu = -1 };  // sched_block_current, sched_wakeup

static const uint32_t priority_quanta[] = { 10, 5, 2, 1 };  // High, Normal, Low, Idle

/*
 * The Idle Task: The ultimate fallback for the CPU when no tasks are ready.
 * It keeps the processor in a low-power state (HLT). On CPU 0, this task 
 * also acts as the 'Reaper', responsible for deallocating ZOMBIE tasks 
 * to prevent memory leaks in the scheduler.
 */
static void idle_task() {
    while (1) {
        if (get_cpu()->cpu_id == 0) {
             sched_reap();
             log_flush();
        }
        for(volatile int i=0; i<500; i++) __asm__ volatile("pause");
        __asm__ volatile("hlt");
    }
}

/*
 *  RUNQUEUE SECTION
 */
void enqueue_task(cpu_context_t* cpu, task_t* task) {
    spin_lock(&cpu->rq_lock);

    task->priority = task->base_priority;
    task_prio_t p = task->priority;

    if (p >= PRIORITY_LEVELS) p = PRIO_NORMAL;
    cpu->current_quanta[p] = 0; 

    task->sched_next = NULL;
    if (cpu->rq_tail[p]) {
        cpu->rq_tail[p]->sched_next = task;
    } else {
        cpu->rq_head[p] = task;
    }
    cpu->rq_tail[p] = task;
    cpu->rq_count[p]++;

    // If task is "homeless" pin to it's CPU
    if (task->cpu_id == (uint64_t)-1) {
        task->cpu_id = cpu->cpu_id;
    }

    spin_unlock(&cpu->rq_lock);
}

task_t* dequeue_task(cpu_context_t* cpu) {
    spin_lock(&cpu->rq_lock);

    for (int p = 0; p < PRIORITY_LEVELS - 1; p++) {
        if (!cpu->rq_head[p]) {
            cpu->current_quanta[p] = 0;
            continue;
        }

        if (cpu->current_quanta[p] >= priority_quanta[p]) {
            bool work_below = false;
            for (int low = p + 1; low < PRIORITY_LEVELS - 1; low++) {
                if (cpu->rq_head[low]) {
                    work_below = true;
                    break;
                }
            }
            if (work_below) {
                cpu->current_quanta[p] = 0;
                continue; 
            }
            cpu->current_quanta[p] = 0;
        }

        task_t* t = cpu->rq_head[p];
        cpu->rq_head[p] = t->sched_next;
        if (!cpu->rq_head[p]) {
            cpu->rq_tail[p] = NULL;
        }
        
        cpu->rq_count[p]--;
        cpu->current_quanta[p]++;

        spin_unlock(&cpu->rq_lock);
        return t;
    }

    spin_unlock(&cpu->rq_lock);
    return NULL;
}

/*
 * Iterates through the local list of sleeping tasks and wakes up those 
 * whose sleep duration has expired. To prevent deadlocks (AB-BA), it first 
 * extracts tasks from the sleeping list under local 'sleep_lock' and then 
 * enqueues them into their respective CPU runqueues.
 */
void sched_update_sleepers() {
    cpu_context_t* cpu = get_cpu();
    uint64_t now = get_uptime_ms();
    
    task_t* ready_queue = NULL;

    uint64_t f = spin_irq_save();
    spin_lock(&cpu->sleep_lock);

    while (cpu->sleeping_list && now >= cpu->sleeping_list->sleep_until) {
        task_t* t = cpu->sleeping_list;
        cpu->sleeping_list = t->sched_next;

        t->sched_next = ready_queue;
        ready_queue = t;
    }

    spin_unlock(&cpu->sleep_lock);

    while (ready_queue) {
        task_t* t = ready_queue;
        ready_queue = t->sched_next;

        t->state = TASK_READY;
        t->sched_next = NULL;

        enqueue_task(cpu, t);
    }
    spin_irq_restore(f);
}

/*
 * Transitions the current task into the TASK_SLEEPING state and calculates 
 * its wake-up time. The task is then inserted into the 'sleeping_task_list' 
 * in a sorted manner (by wake-up time) to allow efficient processing 
 * during scheduler updates.
 */
void sched_make_task_sleep(uint64_t ms) {
    task_t* current = sched_get_current();
    if (!current) return;

    cpu_context_t* cpu = get_cpu();
    current->state = TASK_SLEEPING;
    current->sleep_until = get_uptime_ms() + ms;

    uint64_t f = spin_irq_save();
    spin_lock(&cpu->sleep_lock);

    task_t** pp = &cpu->sleeping_list;

    while (*pp && (*pp)->sleep_until < current->sleep_until) {
        pp = &((*pp)->sched_next);
    }

    current->sched_next = *pp;
    *pp = current;

    spin_unlock(&cpu->sleep_lock);
    spin_irq_restore(f);
}

/*
 * Blocks the current task and adds it to the blocked list with a specific reason.
 * The task remains in TASK_BLOCKED state until a matching sched_wakeup is called.
 */
void sched_block_current(task_reason_t reason) {
    task_t* current = sched_get_current();
    if (!current) return;

    uint64_t f = spin_irq_save();
    spin_lock(&blocked_lock_);

    current->state = TASK_BLOCKED;
    current->wait_reason = reason;
    current->sched_next  = blocked_task_list;
    blocked_task_list    = current;

    spin_unlock(&blocked_lock_);
    spin_irq_restore(f);
}

/*
 * Wakes up all tasks from the blocked list that match the specified reason.
 * Transitions tasks to TASK_READY and re-inserts them into their respective 
 * CPU runqueues. Uses a two-phase approach to minimize lock contention.
 */
void sched_wakeup(task_reason_t reason) {
    task_t* tasks_to_wake = NULL;

    uint64_t f = spin_irq_save();
    spin_lock(&blocked_lock_);

    task_t** pp = &blocked_task_list;

    while (*pp) {
        task_t* t = *pp;
        if (t->wait_reason == reason) {
            *pp = t->sched_next;

            t->sched_next = tasks_to_wake;
            tasks_to_wake = t;
            t->state = TASK_READY;
        } else {
            pp = &((*pp)->sched_next);
        }
    }
    
    spin_unlock(&blocked_lock_);
    spin_irq_restore(f);

    while (tasks_to_wake) {
        task_t* t = tasks_to_wake;
        tasks_to_wake = t->sched_next;

        cpu_context_t* target_cpu = get_cpu_by_id(t->cpu_id);
        if (!target_cpu) target_cpu = get_cpu();
        
        enqueue_task(target_cpu, t);
    }
    
}

/*
 * Allocates and initializes the idle task structure for a specific CPU.
 */
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
    vma_init_task(t);

    return t;
}

/*
 * Attempts to steal a migratable user task (TID >= 10) from another CPU's 
 * runqueue to balance load. Respects priority levels and uses trylock 
 * to avoid deadlocks during synchronization.
 */
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

static void prio_boost(cpu_context_t* cpu) {
    for (int p = PRIO_HIGH; p < PRIO_LOW; p++) {
        if (cpu->rq_head[p + 1] == NULL) continue;

        if (cpu->rq_tail[p]) {
            cpu->rq_tail[p]->sched_next = cpu->rq_head[p + 1];
        } else {
           cpu->rq_head[p] = cpu->rq_head[p + 1];
        }

        cpu->rq_tail[p] = cpu->rq_tail[p + 1];
        cpu->rq_count[p] += cpu->rq_count[p + 1];

        cpu->rq_head[p + 1] = NULL;
        cpu->rq_tail[p + 1] = NULL;
        cpu->rq_count[p + 1] = 0;
    }
}

/*
 * Initializes the scheduler on the Bootstrap Processor (BSP).
 */
void sched_init() {
    cpu_context_t* cpu = get_cpu();
    
    // Convert current execution flow into the 'Main' task (TID 0)
    task_t* main_task = kmalloc(sizeof(task_t));
    memset(main_task, 0, sizeof(task_t));
    
    main_task->tid = 0;
    vma_init_task(main_task);
    main_task->state = TASK_RUNNING;
    main_task->next = main_task;
    main_task->prev = main_task;

    main_task->cpu_id = cpu->cpu_id;
    main_task->is_user = false;
    main_task->cr3 = read_cr3();
    main_task->stack_size = 0;
    
    root_task = main_task;
    cpu->current_task = main_task;
    cpu->idle_task = create_idle_struct(idle_task);
}

/*
 * Core SMP Multi-level Round-Robin Scheduler.
 * Handles context switching, load balancing via work stealing, and task states.
 * frame is a Pointer to the current interrupt stack frame.
 * returns The stack pointer (RSP) of the next task to run.
 */
uint64_t schedule(interrupt_frame_t* frame) {
    uint64_t f = spin_irq_save();
    
    cpu_context_t* cpu = get_cpu();
    task_t* current = cpu->current_task;
    uint64_t now = get_uptime_ms();

    if (now >= cpu->next_priority_boost) {
        prio_boost(cpu);
        cpu->next_priority_boost = now + PRIORITY_BOOST;
    }

    if (current) {
        current->rsp = (uintptr_t)frame;
        if (current && current->state == TASK_RUNNING && current->priority != PRIO_IDLE) {
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

/*
 * Bootstraps the scheduler on an Application Processor (AP).
 * Creates a CPU-specific idle context and sets it as the starting task,
 * allowing the AP to begin accepting work from the global task list.
 */
void sched_init_ap() {
    cpu_context_t* cpu = get_cpu();
    cpu->idle_task = create_idle_struct(idle_task); 
    cpu->current_task = cpu->idle_task;
}

/*
 * Gracefully terminates the current task. Marks it as a ZOMBIE and 
 * moves it to the 'dead_task_list' for asynchronous cleanup.
 * This allows the task to die quickly while the actual memory
 * deallocation is offloaded to the Reaper (CPU 0).
 */
void task_exit() {
    // 1. Get current task
    task_t* current = sched_get_current();
    if (!current) return;

    // 2. Lock scheduler
    uint64_t f = spin_irq_save();
    spin_lock(&dead_lock_);

    kprintf("[SCHED] Task %d is now a ZOMBIE.\n", (int)current->tid);

    // 3. Add to global "cleanup later" list 
    current->state = TASK_ZOMBIE;
    current->cpu_id = (uint64_t)-1;
    current->sched_next = dead_task_list;
    dead_task_list = current;

    spin_unlock(&dead_lock_);
    spin_irq_restore(f);

    // 4. Trigger immediate reschedule via timer interrupt vector
    while(1) { sched_yield(); __asm__ volatile("hlt"); }
}

/*
 * The Kernel Reaper: Executed by the Idle task (typically on CPU 0).
 * It drains the 'dead_task_list', unlinks tasks from the global
 * 'root_task' list, and physically frees their kernel stacks and structures. 
 */
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
    // Repeat that process until to_clean != NULL
    while (to_clean) {
        task_t* next_zombie = to_clean->sched_next;

        uint64_t f = spin_irq_save();
        spin_lock(&sched_lock_);

        task_t* p = to_clean->prev;
        task_t* n = to_clean->next;

        p->next = n;
        n->prev = p;

        // Edge-case scenario
        if (root_task == to_clean) {
            if (n == to_clean) root_task = NULL;
            else root_task = n;
        }

        spin_unlock(&sched_lock_);
        spin_irq_restore(f);

        kprintf("[REAPER] Cleaning up TID %d\n", (int)to_clean->tid);

        // Only proper user tasks (TID >= 10) have full memory maps to destroy
        if (to_clean->tid >= 10) vma_destroy_all(to_clean);

        // 3. Finally free space and update "to_clean" list
        kfree((void*)to_clean->stack_base);
        kfree(to_clean);
        to_clean = next_zombie;
    }
}

task_t* sched_get_current() {
    return get_cpu()->current_task;
}

void sched_yield() {
    __asm__ volatile("int $32");
}
