#include <cpu.h>
#include <vmm.h>
#include <pmm.h>
#include <gdt.h>
#include <std_funcs.h>

//
// INIT BSP - most important at start
//
void cpu_init_bsp() {
    // 1. Alloc context
    cpu_context_t* ctx = (cpu_context_t*)phys_to_virt((uintptr_t)pmm_alloc_frame());
    memset(ctx, 0, sizeof(cpu_context_t));

    // 2. Basic data
    ctx->self = ctx;
    ctx->cpu_id = 0;
    cpu_register_context(ctx);
    ctx->pmm_last_index = 0;

    // 3. Stack
    void* stack = pmm_alloc_frames(4); // 16KB
    ctx->kernel_stack = (uintptr_t)phys_to_virt((uintptr_t)stack + (4 * 4096));

    // 4. GDT and SSE
    gdt_setup_for_cpu(ctx);
    cpu_enable_sse();

    // 5. MSR GS_BASE
    cpu_init_context(ctx);
}

//
// Sets up the Fast System Call (SYSCALL/SYSRET - iretq for now) mechanism for the current CPU.
// Configures MSRs for the kernel entry point, segment selectors, and RFLAGS mask.
// Enables the SCE (System Call Extensions) bit in the EFER register.
//
void cpu_init_syscalls() {
    extern void syscall_entry();
    write_msr(0xC0000082, (uintptr_t)syscall_entry); 

    uint64_t star = ((uint64_t)0x13 << 48) | ((uint64_t)0x08 << 32);
    write_msr(0xC0000081, star);

    write_msr(0xC0000084, 0x202);  

    uint64_t efer = read_msr(0xC0000080);
    write_msr(0xC0000080, efer | 1);  
}
