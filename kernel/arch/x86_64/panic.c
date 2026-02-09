#include <panic.h>
#include <serial.h>
#include <apic.h>
#include <cpu.h>
#include <spinlock.h>

//
// Broadcasts a Halt IPI to all other CPU cores.
// This is typically called during a fatal panic to prevent data corruption.
// Note: This does not halt the current core.
//
void smp_halt_others(void) {
    lapic_broadcast_ipi(IPI_VECTOR_HALT);
}

void panic(const char* message) {
    __asm__ volatile("cli");

    smp_halt_others();

    kprint_raw("\n\n########################################\n");
    kprint_raw("###           KERNEL PANIC           ###\n");
    kprint_raw("########################################\n");
    kprint_raw("CPU ID: "); kprint_hex_raw(get_cpu()->cpu_id);
    kprint_raw("\nREASON: "); kprint_raw(message);
    kprint_raw("\n\nSystem halted permanently.\n");

    for (;;) {
        __asm__ volatile("hlt");
    }
}
