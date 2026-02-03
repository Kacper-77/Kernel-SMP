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
    extern spinlock_t kprint_lock_;

    kprint_lock_.lock = 0;
    kprint_lock_.owner = -1;
    
    __asm__ volatile("cli");
    smp_halt_others();

    kprint("\n\n########################################\n");
    kprint("###           KERNEL PANIC           ###\n");
    kprint("########################################\n");
    kprint("CPU ID: "); kprint_hex(get_cpu()->cpu_id);
    kprint("\nREASON: "); kprint(message);
    kprint("\n\nSystem halted permanently.\n");

    for (;;) {
        __asm__ volatile("hlt");
    }
}
