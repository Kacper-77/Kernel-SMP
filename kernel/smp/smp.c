#include <smp.h>
#include <vmm.h>
#include <pmm.h>
#include <gdt.h>
#include <idt.h>
#include <apic.h>
#include <acpi.h>
#include <std_funcs.h>
#include <serial.h>

// Symbole z binarnego bloba trampoliny
extern uint8_t trampoline_start[];
extern uint8_t trampoline_end[];

 static uint64_t cpu_count = 1;

void kernel_main_ap(cpu_context_t* ctx) {
    cpu_init_context(ctx);
    
    gdt_reload_local();
    idt_init();
    
    vmm_enable_pat(); 
    lapic_init_ap();
    
    kprint("AP "); kprint_hex(ctx->cpu_id); kprint(" is alive!\n");
    
    __asm__ volatile("sti");

    while(1) {
        __asm__ volatile("hlt");
    }
}

void smp_init_cpu(uint8_t lapic_id, uint64_t cpu_id) {
    ap_config_t* config = (ap_config_t*)phys_to_virt(AP_CONFIG_PHYS_ADDR);
    
    // Copy trampoline code
    size_t trampoline_size = (uintptr_t)trampoline_end - (uintptr_t)trampoline_start;
    
    memcpy((void*)phys_to_virt(TRAMPOLINE_PHYS_ADDR), 
           trampoline_start, 
           trampoline_size);

    // Alloc context and pages
    cpu_context_t* ctx = (cpu_context_t*)phys_to_virt((uintptr_t)pmm_alloc_frame());  // !!!!!!!!!!!!!!!!!!!!!
    memset(ctx, 0, sizeof(cpu_context_t));
    ctx->cpu_id = cpu_id;
    ctx->lapic_id = lapic_id;
    ctx->self = ctx;
    
    void* stack = pmm_alloc_frames(4); // 16KB
    ctx->kernel_stack = (uintptr_t)phys_to_virt((uintptr_t)stack + (4 * 4096));

    // Data for trampoline
    config->trampoline_stack = ctx->kernel_stack;
    config->trampoline_pml4 = (uintptr_t)vmm_get_pml4_phys();
    config->trampoline_entry = (uintptr_t)kernel_main_ap;
    config->cpu_context_p = (uintptr_t)ctx;
    config->trampoline_ready = 0;

    __asm__ volatile("mfence" ::: "memory");

    boot_ap(lapic_id, 0x08); // 0x08 -> 0x8000

    // waiting for AP
    while(config->trampoline_ready == 0) __asm__("pause");
}

void smp_init(BootInfo* bi) {
    acpi_rsdp_t* rsdp = (acpi_rsdp_t*)phys_to_virt((uintptr_t)bi->acpi.rsdp);
    acpi_madt_t* madt = (acpi_madt_t*)acpi_find_table(rsdp, "APIC");
    
    uint32_t bsp_lapic_id = lapic_read(LAPIC_ID) >> 24;
    // uint64_t cpu_count = 1;

    uint8_t* ptr = (uint8_t*)madt + sizeof(acpi_madt_t);
    uint8_t* end = (uint8_t*)madt + madt->header.length;

    while (ptr < end) {
        acpi_madt_entry_t* entry = (acpi_madt_entry_t*)ptr;
        if (entry->type == 0) { // Processor Local APIC
            madt_entry_lapic_t* lapic = (madt_entry_lapic_t*)ptr;
            if ((lapic->flags & 1) && (lapic->apic_id != bsp_lapic_id)) {
                smp_init_cpu(lapic->apic_id, cpu_count++);
            }
        }
        ptr += entry->length;
    }
}

void boot_ap(uint32_t apic_id, uint8_t vector) {
    lapic_write(LAPIC_ICR_HIGH, apic_id << 24);
    lapic_write(LAPIC_ICR_LOW, 0x0000C500); // INIT

    for(volatile int i = 0; i < 10000000; i++) __asm__("pause");

    lapic_write(LAPIC_ICR_HIGH, apic_id << 24);
    lapic_write(LAPIC_ICR_LOW, 0x00000600 | vector); // SIPI

    for(volatile int i = 0; i < 1000000; i++) __asm__("pause");
}

uint64_t get_cpu_count_test() {
    return cpu_count;
}
