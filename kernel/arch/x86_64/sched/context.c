#include <sched.h>
#include <pmm.h>
#include <vmm.h>
#include <idt.h>
#include <kmalloc.h>
#include <spinlock.h>
#include <std_funcs.h>
#include <cpu.h>
#include <elf.h>

#include <stddef.h>

static uint64_t next_tid = 10;  // Start TIDs for user/test tasks at 10


//
// Internal helpers to avoid boilerplate
//
static task_t* task_alloc_base() {
    task_t* t = kmalloc(sizeof(task_t));
    if (!t) return NULL;

    memset(t, 0, sizeof(task_t));

    t->stack_size = 0x4000;
    t->stack_base = (uintptr_t)kmalloc(t->stack_size);
    if (!t->stack_base) { kfree(t); return NULL; }

    memset((void*)t->stack_base, 0, t->stack_size);

    t->cpu_id = -1;
    t->state = TASK_READY;
    t->tid = __atomic_fetch_add(&next_tid, 1, __ATOMIC_RELAXED);
    t->priority = PRIO_NORMAL;

    return t;
}

static void task_register(task_t* t) {
    extern task_t* root_task;
    extern spinlock_t sched_lock_;

    uint64_t f = spin_irq_save();
    spin_lock(&sched_lock_);

    t->next = root_task->next;
    root_task->next = t;

    spin_unlock(&sched_lock_);
    spin_irq_restore(f);
    
    t->cpu_id = (uint64_t)-1;
    enqueue_task(get_cpu(), t);
}

//
// Creates a new kernel-mode task.
// Allocates a dedicated 16KB stack and prepares an interrupt frame 
// that allows the scheduler to "return" into the task for the first time.
// The task is marked with CPU affinity -1, allowing it to start on any available core.
//
task_t* arch_task_create(void (*entry_point)(void)) {
    task_t* t = task_alloc_base();
    if (!t) return NULL;

    t->priority = PRIO_HIGH;

    uintptr_t stack_top = (t->stack_base + t->stack_size) & ~0x0FULL;
    
    // Prepare an interrupt frame that 'iretq' will use to "return" into the task
    interrupt_frame_t* frame = (interrupt_frame_t*)(stack_top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(interrupt_frame_t));

    frame->rip    = (uintptr_t)entry_point;
    frame->cs     = 0x08;
    frame->ss     = 0x10;
    frame->rflags = 0x202;

    // Initial RSP/RBP pointing to the top of the stack
    frame->rsp = (uintptr_t)stack_top; 
    frame->rbp = (uintptr_t)stack_top;

    t->rsp = (uintptr_t)frame;

    task_register(t);

    return t;
}

//
// Creates a new user-mode (Ring 3) process.
// This involves:
// - Creating a private PML4 (address space) with kernel mapping mirrored.
// - Mapping physical frames for user code at 0x400000.
// - Mapping a dedicated user stack in high-canonical user space.
// - Setting up segment selectors (0x1B for CS, 0x23 for SS) for Ring 3 transition.
//
task_t* arch_task_create_user(void (*entry_point)(void)) {
    task_t* t = task_alloc_base();
    if (!t) return NULL;

    uintptr_t cr3 = vmm_create_user_pml4();
    if (!cr3) return NULL;

    uintptr_t kstack_top = (t->stack_base + t->stack_size) & ~0x0FULL; 
    page_table_t* pml4_virt = vmm_get_table(cr3);

    // Alloc page for user code (low addr)
    uintptr_t code_phys = (uintptr_t)pmm_alloc_frames(4);
    uintptr_t code_virt = 0x400000;
    vmm_map_range(pml4_virt, code_virt, code_phys, 4 * PAGE_SIZE, 
                PTE_PRESENT | PTE_WRITABLE | PTE_USER);

    memset((void*)phys_to_virt(code_phys), 0, 4 * PAGE_SIZE);
    memcpy((void*)phys_to_virt(code_phys), (void*)entry_point, 4 * PAGE_SIZE);

    // Setup User Stack 
    uintptr_t user_stack_phys = (uintptr_t)pmm_alloc_frames(4);
    uintptr_t user_stack_virt = 0x00007FFFF0000000; 
    vmm_map_range(pml4_virt, user_stack_virt, user_stack_phys, 4 * PAGE_SIZE,
                PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_NX); 

    interrupt_frame_t* frame = (interrupt_frame_t*)(kstack_top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(interrupt_frame_t));

    frame->rip    = code_virt;  // Back to low addr
    frame->cs     = 0x1B;     
    frame->ss     = 0x23;     
    frame->rflags = 0x202; 
    
    frame->rsp = user_stack_virt + (4 * PAGE_SIZE) - 8; 
    frame->rbp = frame->rsp;
    
    t->rsp = (uintptr_t)frame;
    t->cr3 = cr3;
    t->is_user = true;

    task_register(t);

    return t;
}

//
// Creates a new user-mode task from a raw ELF image.
//
// - Creates a fresh user page table (PML4)
// - Loads the ELF into the new address space
// - Allocates and maps a user stack
// - Prepares an interrupt frame for iretq into Ring 3
// - Sets up CR3 and marks the task as user-mode
//
// The task is ready to be scheduled and will begin execution
// at the ELF entry point in user space.
//
task_t* arch_task_spawn_elf(void* elf_raw_data) {
    uintptr_t cr3 = vmm_create_user_pml4();
    if (!cr3) return NULL;

    uintptr_t entry = elf_load(cr3, elf_raw_data);
    if (!entry) return NULL;

    task_t* t = task_alloc_base();
    if (!t) return NULL;

    uintptr_t kstack_top = (t->stack_base + t->stack_size) & ~0x0FULL;
    page_table_t* pml4_virt = vmm_get_table(cr3);

    uintptr_t u_stack_virt = 0x00007FFFF0000000;
    uintptr_t u_stack_phys = (uintptr_t)pmm_alloc_frames(4);
    vmm_map_range(pml4_virt, u_stack_virt, u_stack_phys, 4 * PAGE_SIZE, 
                  PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_NX);

    interrupt_frame_t* frame = (interrupt_frame_t*)(kstack_top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(interrupt_frame_t));

    frame->rip    = entry;
    frame->cs     = 0x1B;
    frame->ss     = 0x23;
    frame->rflags = 0x202;
    frame->rsp    = u_stack_virt + (4 * PAGE_SIZE) - 8;
    frame->rbp    = frame->rsp;

    t->rsp     = (uintptr_t)frame;
    t->cr3     = cr3;
    t->is_user = true;

    task_register(t);

    return t;
}
