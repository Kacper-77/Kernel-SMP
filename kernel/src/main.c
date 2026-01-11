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
#include <cpu.h>
#include <test.h>
#include <kmalloc.h>
#include <spinlock.h>

#include <stdint.h>
#include <stddef.h>

// Global spinlock flag
int g_lock_enabled = 0;

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
    init_serial();
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

    kprint("###   Greetings from Higher Half!   ###\n");
    
    kmalloc_init(); kprint("Heap initialized.\n");

    kprint("Starting Coalescing Test\n");

    void* t1 = kmalloc(100);
    void* t2 = kmalloc(100);
    void* t3 = kmalloc(100);

    kprint("t1: "); kprint_hex((uintptr_t)t1); kprint("\n");
    kprint("t2: "); kprint_hex((uintptr_t)t2); kprint("\n");
    kprint("t3: "); kprint_hex((uintptr_t)t3); kprint("\n");

    kfree(t1);
    kfree(t2);
    
    void* t4 = kmalloc(200);
    kprint("t4 (should reuse t1 space): "); kprint_hex((uintptr_t)t4); kprint("\n");

    if (t4 == t1) {
        kprint("SUCCESS: Coalescing works! t4 reused and merged t1+t2.\n");
    } else {
        kprint("DEBUG: t4 is at: "); kprint_hex((uintptr_t)t4); kprint("\n");
    }

    strcpy((char*)t4, "## DATA SAVED IN T4 ##\n");
    kprint((char*)t4); kprint("\n");

    kmalloc_dump();

    // BSP
    draw_test_squares_safe(1, 
                           (uint32_t*)bi->fb.framebuffer_base, 
                           bi->fb.pixels_per_scanline);

    if (bi->acpi.rsdp != 0) {
        acpi_rsdp_t *rsdp = (acpi_rsdp_t*)phys_to_virt((uintptr_t)bi->acpi.rsdp);
        acpi_madt_t *madt = (acpi_madt_t*)acpi_find_table(rsdp, "APIC");

        if (madt) {
            uintptr_t v_lapic = (uintptr_t)vmm_map_device(vmm_get_pml4(), 
                                                (uintptr_t)phys_to_virt(madt->local_apic_address),
                                                (uintptr_t)madt->local_apic_address, 
                                                4096);
            lapic_init(v_lapic);
            g_lock_enabled = 1;

            lapic_timer_calibrate();
            lapic_timer_init(10, 32);

            kprint("Starting SMP initialization...\n");
            smp_init(bi);

            kmalloc_dump();
        }
    }

    vmm_unmap_range(vmm_get_pml4(), 0x0, 0x20000000);
    kprint("Kernel isolated.\n");

    __asm__ volatile("sti");

    kprint("Testing timer (waiting 5 seconds)...\n");
    uint64_t start = get_uptime_ms();
    while(get_uptime_ms() < start + 5000) {
        __asm__ volatile("pause");
    }
    kprint("Timer TEST PASSED!\n");

    kprint("###   Higher Half kernel is now idling.   ###\n");
    for (;;) { __asm__ __volatile__("hlt"); }
}
