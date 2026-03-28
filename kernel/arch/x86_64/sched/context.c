#include <sched.h>
#include <pmm.h>
#include <vmm.h>
#include <idt.h>
#include <kmalloc.h>
#include <spinlock.h>
#include <std_funcs.h>
#include <cpu.h>
#include <elf.h>
#include <vma.h>

#include <stddef.h>

static uint64_t next_tid = 10;  // Start TIDs for user/test tasks at 10


/*
 * Internal helpers to avoid boilerplate
 */
static task_t* task_alloc_base() {
    task_t* t = kmalloc(sizeof(task_t));
    if (!t) return NULL;

    memset(t, 0, sizeof(task_t));

    vma_init_task(t);

    t->stack_size = 4 * PAGE_SIZE;
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

    task_t* head = root_task;
    task_t* tail = head->prev;

    t->next = head;
    t->prev = tail;

    tail->next = t;
    head->prev = t;

    spin_unlock(&sched_lock_);
    spin_irq_restore(f);
    
    t->cpu_id = (uint64_t)-1;
    enqueue_task(get_cpu(), t);
}

/*
 * Creates a new kernel-mode task.
 * Allocates a dedicated 16KB stack and prepares an interrupt frame 
 * that allows the scheduler to "return" into the task for the first time.
 * The task is marked with CPU affinity -1, allowing it to start on any available core.
 */
task_t* arch_task_create(void (*entry_point)(void)) {
    task_t* t = task_alloc_base();
    if (!t) return NULL;

    t->priority = PRIO_HIGH;
    t->is_user  = false;
    t->cr3      = read_cr3();

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

/*
 * Creates a new user-mode (Ring 3) process from a function pointer.
 */
task_t* arch_task_create_user(void (*entry_point)(void)) {
    task_t* t = task_alloc_base();
    if (!t) return NULL;

    uintptr_t cr3 = vmm_create_user_pml4();
    if (!cr3) { kfree((void*)t->stack_base); kfree(t); return NULL; }
    t->cr3 = cr3;
    t->is_user = true;

    // 1. Map User Code via VMA (Allocates physical memory and registers it)
    uintptr_t code_virt = 0x400000;
    uint64_t code_size = 4 * PAGE_SIZE;
    if (vma_map(t, code_virt, code_size, VMA_READ | VMA_EXEC | VMA_USER) != 0) return NULL;

    // Copy the code from entry_point to the newly allocated physical pages
    uintptr_t code_phys = vmm_virtual_to_physical(vmm_get_table(cr3), code_virt);
    memcpy((void*)phys_to_virt(code_phys), (void*)entry_point, code_size);

    // 2. Map User Stack and Heap via VMA
    uintptr_t u_stack_virt = 0x00007FFFFFFFF000 - (4 * PAGE_SIZE);
    uint64_t u_stack_size = 4 * PAGE_SIZE;
    if (vma_map(t, u_stack_virt, u_stack_size, VMA_READ | VMA_WRITE | VMA_USER | VMA_STACK) != 0) return NULL;

    // 3. Initialize Heap Pointer
    t->heap_start = 0x406000;
    t->heap_curr  = t->heap_start;

    // Setup the interrupt frame for Ring 3 transition (iretq)
    uintptr_t kstack_top = (t->stack_base + t->stack_size) & ~0x0FULL; 
    interrupt_frame_t* frame = (interrupt_frame_t*)(kstack_top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(interrupt_frame_t));

    frame->rip    = code_virt;
    frame->cs     = 0x1B; // User Code Selector (RPL 3)
    frame->ss     = 0x23; // User Data Selector (RPL 3)
    frame->rflags = 0x202; 
    frame->rsp    = u_stack_virt + u_stack_size - 8;
    frame->rbp    = frame->rsp;
    
    t->rsp = (uintptr_t)frame;
    task_register(t);

    return t;
}

/* 
 * Creates a new user-mode task from a raw ELF image.
 */
task_t* arch_task_spawn_elf(void* elf_raw_data) {
    task_t* t = task_alloc_base();
    if (!t) return NULL;

    uintptr_t cr3 = vmm_create_user_pml4();
    if (!cr3) { kfree((void*)t->stack_base); kfree(t); return NULL; }
    t->cr3 = cr3;
    t->is_user = true;

    // 1. Load ELF segments into VMA structures
    uintptr_t entry = elf_load(t, elf_raw_data); 
    if (!entry) return NULL;

    // 2. Map User Stack and Heap via VMA
    uintptr_t u_stack_virt = 0x00007FFFFFFFF000 - (4 * PAGE_SIZE);
    uint64_t u_stack_size = 4 * PAGE_SIZE;
    if (vma_map(t, u_stack_virt, u_stack_size, VMA_READ | VMA_WRITE | VMA_USER | VMA_STACK) != 0) return NULL;
    
    t->heap_curr = t->heap_start;

    // 3. Setup Kernel Stack Frame
    uintptr_t kstack_top = (t->stack_base + t->stack_size) & ~0x0FULL;
    interrupt_frame_t* frame = (interrupt_frame_t*)(kstack_top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(interrupt_frame_t));

    frame->rip    = entry;
    frame->cs     = 0x1B;
    frame->ss     = 0x23;
    frame->rflags = 0x202;
    frame->rsp    = u_stack_virt + u_stack_size - 8;
    frame->rbp    = frame->rsp;

    t->rsp = (uintptr_t)frame;
    task_register(t);

    return t;
}
