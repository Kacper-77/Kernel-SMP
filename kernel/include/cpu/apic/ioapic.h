#ifndef IOAPIC_H
#define IOAPIC_H

#include <stdint.h>

// IOAPIC REGS
#define IOAPIC_REG_ID     0x00
#define IOAPIC_REG_VER    0x01
#define IOAPIC_REG_ARB    0x02
#define IOAPIC_REG_TABLE  0x10  // Redirection Table

void ioapic_write(uint8_t reg, uint32_t data);
uint32_t ioapic_read(uint8_t reg);

void ioapic_init(uintptr_t base_addr);
void ioapic_set_irq(uint8_t irq, uint8_t apic_id, uint8_t vector);

#endif