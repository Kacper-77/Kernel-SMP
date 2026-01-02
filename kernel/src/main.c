#include <boot_info.h>
#include <serial.h>
#include <acpi.h>
#include <gdt.h>
#include <idt.h>
#include <pmm.h>
#include <vmm.h>
#include <apic.h>
#include <smp.h>

#include <stdint.h>
#include <stddef.h>

// Forward declaration of the high-half entry point
void kernel_main_high(BootInfo *bi);

static inline uint32_t pack_rgb(uint8_t r, uint8_t g, uint8_t b,
                                uint32_t rm, uint32_t gm, uint32_t bm) {
    if (rm == 0 && gm == 0 && bm == 0) return (r << 16) | (g << 8) | b;
    int rs = __builtin_ctz(rm), gs = __builtin_ctz(gm), bs = __builtin_ctz(bm);
    return ((uint32_t)r << rs) | ((uint32_t)g << gs) | ((uint32_t)b << bs);
}

// Low-half entry point (Bootstrap)
__attribute__((sysv_abi, section(".text.entry")))
void kernel_main(BootInfo *bi) {
    __asm__ volatile("cli");
    pmm_init(bi);
    vmm_init(bi); 

    __asm__ __volatile__ (
        "movabs $kernel_main_high, %%rax \n\t"
        "jmp *%%rax"
        : : "D"(bi) : "rax"
    );

    while(1);
}

// High-half entry point
void kernel_main_high(BootInfo *bi) {
    gdt_init();
    idt_init();
    init_serial();
    kprint("###   Greetings from Higher Half!   ###\n");

    // Re-verify PMM
    void* frame1 = pmm_alloc_frame();
    kprint("PMM Frame Test: "); kprint_hex((uintptr_t)frame1); kprint("\n");

    uint32_t rm = bi->fb.red_mask;
    uint32_t gm = bi->fb.green_mask;
    uint32_t bm = bi->fb.blue_mask;
    
    // Using the high virtual address for framebuffer
    volatile uint32_t *fb = (volatile uint32_t*)bi->fb.framebuffer_base;
    kprint("FB Virt: "); kprint_hex((uintptr_t)fb); kprint("\n");
    kprint("FB Size: "); kprint_hex((uintptr_t)bi->fb.framebuffer_size); kprint("\n");

    uint32_t stride = bi->fb.pixels_per_scanline;

    uint32_t cpu_count = 0;

    if (bi->acpi.rsdp != 0) {
        // kprint("RSDP Phys: "); kprint_hex((uintptr_t)bi->acpi.rsdp); kprint("\n");
        acpi_rsdp_t *rsdp = (acpi_rsdp_t*)phys_to_virt((uintptr_t)bi->acpi.rsdp);
        kprint("RSDP Virt: "); kprint_hex((uintptr_t)rsdp); kprint("\n");

        acpi_madt_t *madt = (acpi_madt_t*)acpi_find_table(rsdp, "APIC");

        if (madt) {
            uintptr_t v_lapic = (uintptr_t)vmm_map_device(vmm_get_pml4(), 
                                                (uintptr_t)phys_to_virt(madt->local_apic_address),
                                                (uintptr_t)madt->local_apic_address, 
                                                4096);
            lapic_init(v_lapic);

            // SMP first test - not passed in 100% (more like 60%)
            kprint("Starting SMP initialization...\n");
            smp_init(bi);

            cpu_count = get_cpu_count_test();
            // cpu_count++;
            
            uint32_t id = lapic_read(LAPIC_ID);
            kprint("APIC ID: "); kprint_hex(id >> 24); kprint("\n");

            // uint8_t *ptr = (uint8_t *)madt + sizeof(acpi_madt_t);
            // uint8_t *end = (uint8_t *)madt + madt->header.length;

            // while (ptr < end) {
            //     acpi_madt_entry_t *entry = (acpi_madt_entry_t *)ptr;
            //     if (entry->type == 0) {
            //         madt_entry_lapic_t *lapic = (madt_entry_lapic_t *)ptr;
            //         if (lapic->flags & 1) cpu_count++;
            //     }
            //     ptr += entry->length;
            // }
        }
    }

    // Visual feedback
    uint32_t blue = pack_rgb(0, 0, 255, rm, gm, bm);
    for (uint32_t i = 0; i < cpu_count; i++) {
        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < 20; x++) {
                fb[(450 * stride) + (40 + i * 30) + x + (y * stride)] = blue;
            }
        }
    }

    kprint("###   Higher Half kernel is now idling.   ###\n");
    for (;;) { __asm__ __volatile__("hlt"); }
}
