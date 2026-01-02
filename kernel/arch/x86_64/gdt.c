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

// GDT layout: [0] Null, [1] Kernel Code, [2] Kernel Data...
static struct gdt_entry gdt[35];
static struct gdt_ptr gdtr;

//
// External assembly helper to flush segment registers.
// After lgdt, we must perform a far return or far jump to update CS,
// and manually reload data segments (DS, ES, SS, FS, GS).
//
extern void gdt_flush(uint64_t gdtr_ptr);

//
// Helper
//
void gdt_set_entry(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access      = access;
}

void gdt_reload_local() {
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uintptr_t)&gdt;
    gdt_flush((uintptr_t)&gdtr);
}

//
// INIT
//
void gdt_init() {
    for(int i = 0; i < 35; i++) {
        gdt[i] = (struct gdt_entry){0, 0, 0, 0, 0, 0};
    }

    // 0x00: Null
    gdt_set_entry(0, 0, 0, 0, 0);

    // 0x08: Kernel Code (Long Mode: Access 0x9A, Granularity 0x20)
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0x20);

    // 0x10: Kernel Data (Access 0x92, Granularity 0x00)
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0x00);

    gdt_reload_local();
}