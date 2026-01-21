#include <process.h>

#include <cpu.h>
#include <pmm.h>
#include <vmm.h>
#include <serial.h>
#include <kmalloc.h>

void start_user_process(void* entry_point) {
    size_t stack_size = 16384;
    uintptr_t user_stack_phys = (uintptr_t)pmm_alloc_frames(4); 
    uintptr_t user_stack_virt = phys_to_virt((uintptr_t)user_stack_phys);

    // Map the stack with User flag
    vmm_map_range(vmm_get_pml4(), 
                 user_stack_virt, 
                 user_stack_phys, 
                 stack_size, 
                 PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_NX);

    cpu_context_t* cpu = get_cpu();
    cpu->tss.rsp0 = cpu->kernel_stack; 

    kprint("[KERNEL] Jumping to Ring 3...\n");

    extern void jump_to_user(void* rip, void* rsp);
    // Leap of faith
    jump_to_user(entry_point, (void*)((uintptr_t)user_stack_virt + stack_size));
}
