#include <panic.h>
#include <serial.h>
#include <apic.h>
#include <cpu.h>

void smp_halt_others(void) {
    lapic_broadcast_ipi(IPI_VECTOR_HALT);
}

void panic(const char* message) {
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
