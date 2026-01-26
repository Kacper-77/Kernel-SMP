#include <cpu.h>
#include <vmm.h>
#include <pmm.h>
#include <gdt.h>
#include <std_funcs.h>

//
// INIT BSP
//
void cpu_init_bsp() {
    // 1. Alloc context
    cpu_context_t* ctx = (cpu_context_t*)phys_to_virt((uintptr_t)pmm_alloc_frame());
    memset(ctx, 0, sizeof(cpu_context_t));

    // 2. Basic data
    ctx->self = ctx;
    ctx->cpu_id = 0;
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

void cpu_init_syscalls() {
    extern void syscall_entry();
    write_msr(0xC0000082, (uintptr_t)syscall_entry); // LSTAR

    uint64_t star = ((uint64_t)0x0B << 48) | ((uint64_t)0x08 << 32);
    write_msr(0xC0000081, star);

    write_msr(0xC0000084, 0x202);  // SFMASK

    uint64_t efer = read_msr(0xC0000080);
    write_msr(0xC0000080, efer | 1);  // SCE bit
}
