#include <boot_info.h>
#include <serial.h>
#include <acpi.h>
#include <gdt.h>
#include <idt.h>
#include <pmm.h>
#include <vmm.h>
#include <apic.h>
#include <smp.h>
#include <std_funcs.h>

#include <test.h>

#include <stdint.h>
#include <stddef.h>

//
// INIT BSP - WILL BE MOVED
//
static inline void cpu_init_bsp() {
    // 1. Alloc context
    cpu_context_t* ctx = (cpu_context_t*)phys_to_virt((uintptr_t)pmm_alloc_frame());
    memset(ctx, 0, sizeof(cpu_context_t));

    // 2. Basic data
    ctx->self = ctx;
    ctx->cpu_id = 1;
    ctx->lapic_id = 0;
    ctx->pmm_last_index = 0;

    // 3. Stack
    uint64_t current_rsp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(current_rsp));
    ctx->kernel_stack = current_rsp;

    // 4. MSR GS_BASE
    cpu_init_context(ctx);

    // 5. GDT and SSE
    gdt_setup_for_cpu(ctx);
    cpu_enable_sse();
}

// Forward declaration
void kernel_main_high(BootInfo *bi);

// Low-half entry point (Bootstrap)
__attribute__((sysv_abi, section(".text.entry")))
void kernel_main(BootInfo *bi) {
    __asm__ volatile(
        "and $-16, %%rsp\n\t"
        "sub $8, %%rsp"
        ::: "rsp"
    );
    __asm__ volatile("cli");
    pmm_init(bi);
    vmm_init(bi);

    BootInfo* bi_virt = (BootInfo*)phys_to_virt((uintptr_t)bi);

    __asm__ __volatile__ (
        "movabs $kernel_main_high, %%rax \n\t"
        "jmp *%%rax"
        : : "D"(bi_virt) : "rax"
    );

    while(1);
}

// High-half entry point
void kernel_main_high(BootInfo *bi) {  
    cpu_init_bsp();
    
    idt_init();
    init_serial();

    kprint("###   Greetings from Higher Half!   ###\n");

    if (bi->acpi.rsdp != 0) {
        acpi_rsdp_t *rsdp = (acpi_rsdp_t*)phys_to_virt((uintptr_t)bi->acpi.rsdp);
        acpi_madt_t *madt = (acpi_madt_t*)acpi_find_table(rsdp, "APIC");

        if (madt) {
            uintptr_t v_lapic = (uintptr_t)vmm_map_device(vmm_get_pml4(), 
                                                (uintptr_t)phys_to_virt(madt->local_apic_address),
                                                (uintptr_t)madt->local_apic_address, 
                                                4096);
            lapic_init(v_lapic);
            
            kprint("Starting SMP initialization...\n");
            smp_init(bi);
        }
    }

    vmm_unmap_range(vmm_get_pml4(), 0x0, 0x20000000);
    kprint("Kernel isolated.\n");

    // BSP
    draw_test_squares_safe(1, 
                           (uint32_t*)bi->fb.framebuffer_base, 
                           bi->fb.pixels_per_scanline);

    kprint("###   Higher Half kernel is now idling.   ###\n");
    for (;;) { __asm__ __volatile__("hlt"); }
}
