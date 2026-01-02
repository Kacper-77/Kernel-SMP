#ifndef GDT_H
#define GDT_H

#include <stdint.h>

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

struct tss {
    uint32_t reserved0;
    uint64_t rsp0;      // Used when Ring 3 -> Ring 0
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];    // Interrupt Stack Table
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

//
// Segment selectors offsets in our GDT.
// These are used to load segment registers (CS, DS, etc.)
//
#define KERNEL_CODE_SEL 0x08
#define KERNEL_DATA_SEL 0x10

//
// Initializes the Global Descriptor Table.
// Sets up Null, Kernel Code, and Kernel Data segments,
// then flushes the segment registers.
//
void gdt_init(void);
void gdt_reload_local();

// Helper
void gdt_set_entry(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);

#endif