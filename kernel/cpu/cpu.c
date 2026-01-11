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
    uint64_t current_rsp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(current_rsp));
    ctx->kernel_stack = current_rsp;

    // 4. GDT and SSE
    gdt_setup_for_cpu(ctx);
    cpu_enable_sse();

    // 5. MSR GS_BASE
    cpu_init_context(ctx);
}
