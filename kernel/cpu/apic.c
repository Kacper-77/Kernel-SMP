#include <pit.h>
#include <cpu.h>
#include <vmm.h>
#include <apic.h>
#include <serial.h>
#include <panic.h>

// Pointer to the memory-mapped APIC registers
volatile uint32_t* lapic_base = NULL;
volatile uint64_t global_ticks_per_ms = 0;

// Write a 32-bit value to a Local APIC register.
void lapic_write(uint32_t reg, uint32_t data) {
    *(volatile uint32_t*)((uintptr_t)lapic_base + reg) = data;
}

// Read a 32-bit value from a Local APIC register.
uint32_t lapic_read(uint32_t reg) {
    return *(volatile uint32_t*)((uintptr_t)lapic_base + reg);
}

//
// Initialize the Local APIC for the current CPU
//
void lapic_init(uintptr_t virt_addr) {
    lapic_base = (uint32_t*)virt_addr;

    // 1. Clear Error Status Register
    lapic_write(LAPIC_ESR, 0);
    lapic_write(LAPIC_ESR, 0);

    // 2. Enable the Local APIC
    lapic_write(LAPIC_SVR, lapic_read(LAPIC_SVR) | LAPIC_SVR_ENABLE | 0xFF);

    // 3. Set Task Priority Register to 0
    lapic_write(LAPIC_TPR, 0);

    // 4. EOI (End of Interrupt)
    lapic_write(LAPIC_EOI, 0);
}

void lapic_init_ap() {
    lapic_write(LAPIC_SVR, lapic_read(LAPIC_SVR) | 0x1FF);

    lapic_write(LAPIC_TPR, 0);

    lapic_write(LAPIC_ESR, 0);
    lapic_write(LAPIC_ESR, 0);
}

//
// Calibrates the Local APIC timer frequency using the legacy PIT as a reference.
// The BSP performs the actual measurement (10ms window), while APs wait for the result
// to ensure synchronized timekeeping across all 32 cores.
//
void lapic_timer_calibrate() {
    cpu_context_t* cpu = get_cpu();

    if (cpu->cpu_id == 0) {
        // Only BSP can calibrate
        lapic_write(LAPIC_TDCR, 0x03);
        lapic_write(LAPIC_TICR, 0xFFFFFFFF);

        pit_prepare_sleep(11932);
        pit_wait_calibration();

        uint32_t current_ticks = lapic_read(LAPIC_TCCR);
        global_ticks_per_ms = (0xFFFFFFFF - current_ticks) / 10;
    } else {
        // APs waits
        while (global_ticks_per_ms == 0) {
           __asm__ volatile("pause");
        }
    }

    // Save tics
    cpu->lapic_ticks_per_ms = global_ticks_per_ms;

    lapic_write(LAPIC_TICR, 0);

    kprint("CPU "); kprint_hex(cpu->cpu_id);
    kprint(" used global calibration: "); kprint_hex(cpu->lapic_ticks_per_ms);
    kprint(" ticks/ms\n");
}

//
// Enables the Local APIC timer in periodic mode for the current CPU.
// Uses the pre-calculated calibration values to trigger interrupts 
// at the specified millisecond interval on the given IDT vector.
//
void lapic_timer_init(uint32_t ms_interval, uint8_t vector) {
    cpu_context_t* cpu = get_cpu();
    
    // 1. Configure LVT Timer Register
    // Bit 17: 1 for Periodic Mode, 0 for One-Shot
    // Bits 0-7: Interrupt Vector
    lapic_write(LAPIC_LVT_TIMER, vector | (1 << 17));

    // 2. Set Divider to 16 (must match calibration)
    lapic_write(LAPIC_TDCR, 0x03);

    // 3. Set Initial Count to the calculated ticks for the requested interval
    lapic_write(LAPIC_TICR, cpu->lapic_ticks_per_ms * ms_interval);
}

//
// Waits for the Local APIC to finish delivering the previous interrupt (IPI).
// Checks the Delivery Status bit in the ICR. Includes a safety timeout 
// to prevent infinite hangs if the APIC bus becomes unresponsive.
//
void lapic_wait_for_delivery() {
    uint64_t timeout = 1000000;

    while (lapic_read(LAPIC_ICR_LOW) & ICR_SEND_PENDING) {
        __asm__ volatile("pause");
        timeout--;
    }

    if (timeout == 0) 
        panic("FATAL: LAPIC IPI delivery stuck!\n");
}

//
// Allows to send interrupt to core (by it's ID)
//
void lapic_send_ipi(uint8_t target_lapic_id, uint8_t vector) {
    lapic_wait_for_delivery();
    
    // Dest core: ICR High (bits 24-31)
    lapic_write(LAPIC_ICR_HIGH, (uint32_t)target_lapic_id << 24);
    
    // Send command through ICR Low
    lapic_write(LAPIC_ICR_LOW, ICR_ASSERT | ICR_EDGE | ICR_FIXED | vector);
}

//
// Send interrupt to all cores, except one who called it.
//
void lapic_broadcast_ipi(uint8_t vector) {
    lapic_wait_for_delivery();
    lapic_write(LAPIC_ICR_LOW, ICR_SHORTHAND_OTHERS | ICR_ASSERT | ICR_EDGE | ICR_FIXED | vector);
}
