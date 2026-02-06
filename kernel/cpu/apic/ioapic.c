#include <ioapic.h>
#include <serial.h>
#include <vmm.h>

static volatile uint32_t* ioapic_base;

#define IOREGSEL 0
#define IOWIN    4

void ioapic_write(uint8_t reg, uint32_t data) {
    ioapic_base[IOREGSEL] = reg;
    ioapic_base[IOWIN] = data;
}

uint32_t ioapic_read(uint8_t reg) {
    ioapic_base[IOREGSEL] = reg;
    return ioapic_base[IOWIN];
}

void ioapic_set_irq(uint8_t pin, uint8_t apic_id, uint8_t vector) {
    uint32_t low_index = 0x10 + (pin * 2);
    uint32_t high_index = 0x11 + (pin * 2);

    ioapic_write(high_index, (uint32_t)apic_id << 24);
    ioapic_write(low_index, vector & ~(1 << 16)); 
}

void ioapic_init(uintptr_t phys_addr) {
    uintptr_t virt_addr = phys_to_virt(phys_addr);
    ioapic_base = (uint32_t*)virt_addr;

    vmm_map(vmm_get_pml4(), virt_addr, phys_addr, PTE_PRESENT | PTE_WRITABLE | PTE_NX | PTE_PCD); 

    kprint("IOAPIC: Mapped "); kprint_hex(phys_addr); 
    kprint(" to "); kprint_hex(virt_addr); kprint("\n");

    uint32_t ver_reg = ioapic_read(0x01);
    uint32_t max_entries = ((ver_reg >> 16) & 0xFF) + 1;

    for (uint32_t i = 0; i < max_entries; i++) {
        ioapic_write(0x10 + i * 2, 0x00010000); 
        ioapic_write(0x10 + i * 2 + 1, 0);
    }
}
