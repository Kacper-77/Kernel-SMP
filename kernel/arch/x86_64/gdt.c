#include <gdt.h>

#include <stdint.h>

//
// GDT Entry structure
// In x86_64, Base and Limit are mostly ignored in Long Mode for code/data,
// but the L-bit (Long mode) and DPL (Privilege level) are crucial.
//
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

//
// GDTR structure - the pointer passed to the 'lgdt' instruction
//
struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// GDT layout: [0] Null, [1] Kernel Code, [2] Kernel Data
static struct gdt_entry gdt[3];
static struct gdt_ptr gdtr;

//
// External assembly helper to flush segment registers.
// After lgdt, we must perform a far return or far jump to update CS,
// and manually reload data segments (DS, ES, SS, FS, GS).
//
extern void gdt_flush(uint64_t gdtr_ptr);

void gdt_init() {
    // 0x00: Null Descriptor - Always required
    gdt[0] = (struct gdt_entry){0, 0, 0, 0, 0, 0};

    //
    // 0x08: Kernel Code Segment
    // Access 0x9A: 10011010b (Present, Ring 0, Code, Executable, Readable)
    // Granularity 0x20: 00100000b (L-bit set for 64-bit Long Mode)
    //
    gdt[1] = (struct gdt_entry){0, 0, 0, 0x9A, 0x20, 0};

    //
    // 0x10: Kernel Data Segment
    // Access 0x92: 10010010b (Present, Ring 0, Data, Writable)
    // Granularity 0x00: (L-bit not needed for data)
    //
    gdt[2] = (struct gdt_entry){0, 0, 0, 0x92, 0x00, 0};

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uintptr_t)&gdt;

    // Load the GDT and refresh segments
    // We use an assembly helper for a clean CS reload
    gdt_flush((uintptr_t)&gdtr);
}