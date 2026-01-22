#include <sched.h>
#include <pmm.h>
#include <vmm.h>
#include <kmalloc.h>
#include <spinlock.h>
#include <std_funcs.h>

static uint64_t next_tid = 10;  // Start TIDs for user/test tasks at 10


//
// Architecture-specific task creation. 
// Sets up the initial stack and interrupt frame.
//
task_t* arch_task_create(void (*entry_point)(void)) {
    extern task_t* root_task;
    extern spinlock_t sched_lock_;

    // Allocate task control block
    task_t* t = kmalloc(sizeof(task_t));
    if (!t) return NULL;
    memset(t, 0, sizeof(task_t));
    
    // Allocate 16KB kernel stack for the task
    uint64_t stack_size = 0x4000;
    t->stack_base = (uintptr_t)kmalloc(stack_size);
    if (!t->stack_base) {
        kfree(t);
        return NULL;
    }

    uintptr_t stack_top = (t->stack_base + stack_size) & ~0x0FULL;
    
    // Prepare an interrupt frame that 'iretq' will use to "return" into the task
    interrupt_frame_t* frame = (interrupt_frame_t*)(stack_top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(interrupt_frame_t));

    frame->rip = (uintptr_t)entry_point;
    frame->cs = 0x08;
    frame->ss = 0x10;
    frame->rflags = 0x202; // IF=1

    // Initial RSP/RBP pointing to the top of the stack
    frame->rsp = (uintptr_t)(stack_top - 8); 
    frame->rbp = (uintptr_t)(stack_top - 8);

    t->rsp = (uintptr_t)frame;
    t->state = TASK_READY;
    t->cpu_id = -1;

    // Lock scheduler to safely modify the global circular list
    spin_lock(&sched_lock_);
    
    t->tid = next_tid++;
    t->next = root_task->next;
    root_task->next = t;
    
    spin_unlock(&sched_lock_);

    return t;
}

//
// Architecture-specific user task creation.
// Sets up a private address space, user stack, and kernel stack.
//
task_t* arch_task_create_user(void (*entry_point)(void)) {
    extern task_t* root_task;
    extern spinlock_t sched_lock_;

    task_t* t = kmalloc(sizeof(task_t));
    if (!t) return NULL;
    memset(t, 0, sizeof(task_t));

    // Allocate Kernel Stack (used when entering Ring 0 from Ring 3)
    t->stack_size = 0x4000;
    t->stack_base = (uintptr_t)kmalloc(t->stack_size);
    if (!t->stack_base) {
        kfree(t);
        return NULL;
    }
    uintptr_t kstack_top = (t->stack_base + t->stack_size) & ~0x0FULL;

    // Create a new Page Table (PML4) for the user process
    // This clones the kernel mappings so the task can still "see" the kernel
    t->cr3 = (uintptr_t)vmm_create_user_pml4();
    if (!t->cr3) {
        kfree((void*)t->stack_base);
        kfree(t);
        return NULL;
    }

    // Setup User Stack
    uintptr_t user_stack_phys = (uintptr_t)pmm_alloc_frame();
    uintptr_t user_stack_virt = 0x00007FFFF0000000; 
    
    // Map the user stack into the task's private address space
    // Flags: Present + Writable + User
    page_table_t* pml4_virt = vmm_get_table(t->cr3);
    vmm_map(pml4_virt, user_stack_virt, user_stack_phys, 0x07); 

    // Prepare the Interrupt Frame on the KERNEL stack
    // When the scheduler switches to this task, iretq will pop these values
    interrupt_frame_t* frame = (interrupt_frame_t*)(kstack_top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(interrupt_frame_t));

    frame->rip = (uintptr_t)entry_point;
    frame->cs  = 0x1B;     // User Code Selector (Index 3, RPL 3)
    frame->ss  = 0x23;     // User Data Selector (Index 4, RPL 3)
    frame->rflags = 0x202; // IF=1 (Enable interrupts)
    
    // The user stack pointer the CPU will switch to upon iretq
    frame->rsp = user_stack_virt + 0x1000; 
    frame->rbp = frame->rsp;

    t->rsp = (uintptr_t)frame;
    t->is_user = true;
    t->state = TASK_READY;
    t->cpu_id = -1;

    // Add to the scheduler's task list
    spin_lock(&sched_lock_);
    t->tid = next_tid++;
    t->next = root_task->next;
    root_task->next = t;
    spin_unlock(&sched_lock_);

    return t;
}
