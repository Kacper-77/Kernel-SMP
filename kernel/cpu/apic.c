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
void lapic_init(uintptr_t virt_addr) {
    lapic_base = (uint32_t*)virt_addr;

    // 2. Clear Error Status Register
    lapic_write(LAPIC_ESR, 0);
    lapic_write(LAPIC_ESR, 0);

    // 3. Enable the Local APIC
    lapic_write(LAPIC_SVR, lapic_read(LAPIC_SVR) | LAPIC_SVR_ENABLE | 0xFF);

    // 4. Set Task Priority Register to 0
    lapic_write(LAPIC_TPR, 0);

    // 5. EOI (End of Interrupt)
    lapic_write(LAPIC_EOI, 0);

    kprint("APIC: LAPIC initialized at virtual: ");
    kprint_hex((uint64_t)lapic_base);
    kprint("\n");
}
