#ifndef SMP_H
#define SMP_H

#include <cpu.h>
#include <boot_info.h> 

#include <stdint.h>

#define TRAMPOLINE_PHYS_ADDR 0x8000
#define AP_CONFIG_PHYS_ADDR  0x7000

typedef struct {
    uint64_t trampoline_stack;            // offset 0
    uint64_t trampoline_pml4;             // offset 8 (Physical)
    uint64_t trampoline_entry;            // offset 16 (Virtual)
    uint64_t cpu_context_p;               // offset 24 (Virtual)
    volatile uint64_t trampoline_ready;   // offset 32
} __attribute__((packed)) ap_config_t;

void smp_init(BootInfo* bi);

void smp_init_cpu(uint8_t lapic_id, uint64_t cpu_id);
void boot_ap(uint32_t apic_id, uint8_t vector);

uint64_t get_cpu_count_test();

#endif