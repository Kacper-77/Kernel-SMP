#ifndef GDT_H
#define GDT_H

#include <cpu.h>
#include <stdint.h>

// GDT Descriptor
struct gdt_entry_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

// TSS Descriptor
struct gdt_tss_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper32;
    uint32_t reserved;
} __attribute__((packed));

//
// Initializes the Descriptor Table.
// Sets up Null, Kernel Code, and Kernel Data segments,
// then flushes the segment registers.
//
void gdt_setup_for_cpu(cpu_context_t* ctx);

#endif