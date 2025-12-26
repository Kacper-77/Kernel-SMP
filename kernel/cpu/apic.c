#include <apic.h>
#include <vmm.h>
#include <serial.h>

// Pointer to the memory-mapped APIC registers
static volatile uint32_t* lapic_base = NULL;

// Write a 32-bit value to a Local APIC register.
void lapic_write(uint32_t reg, uint32_t data) {
    *(volatile uint32_t*)((uintptr_t)lapic_base + reg) = data;
}

// Read a 32-bit value from a Local APIC register.
uint32_t lapic_read(uint32_t reg) {
    return *(volatile uint32_t*)((uintptr_t)lapic_base + reg);
}

//
// Send End-of-Interrupt signal to the LAPIC.
// Must be called at the end of every interrupt handler.
//
void lapic_send_eoi() {
    lapic_write(LAPIC_EOI, 0);
}

//
// Initialize the Local APIC for the current CPU
//
void lapic_init(uint64_t phys_addr) {
    // 1. Map the LAPIC registers into virtual memory
    // Even if using identity mapping, we ensure the page is present and writable
    vmm_map_range(vmm_get_pml4(), phys_addr, phys_addr, 4096, PTE_PRESENT | PTE_WRITABLE);
    
    lapic_base = (uint32_t*)phys_addr;

    // 2. Clear Error Status Register (requires two writes on some CPUs)
    lapic_write(LAPIC_ESR, 0);
    lapic_write(LAPIC_ESR, 0);

    // 3. Enable the Local APIC by setting the Spurious Interrupt Vector
    // We map the spurious interrupt to vector 0xFF and set the enable bit
    lapic_write(LAPIC_SVR, lapic_read(LAPIC_SVR) | LAPIC_SVR_ENABLE | 0xFF);

    // 4. Set Task Priority Register to 0 to allow all interrupts
    lapic_write(LAPIC_TPR, 0);

    kprint("APIC Local APIC initialized at physical: ");
    kprint_hex(phys_addr);
    kprint("\n");
}
