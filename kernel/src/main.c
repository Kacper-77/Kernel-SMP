#include <boot_info.h>
#include <serial.h>
#include <acpi.h>
#include <stdint.h>
#include <stddef.h>

static inline uint32_t pack_rgb(uint8_t r, uint8_t g, uint8_t b,
                                uint32_t rm, uint32_t gm, uint32_t bm) {
    if (rm == 0 && gm == 0 && bm == 0) return (r << 16) | (g << 8) | b;
    int rs = __builtin_ctz(rm), gs = __builtin_ctz(gm), bs = __builtin_ctz(bm);
    return ((uint32_t)r << rs) | ((uint32_t)g << gs) | ((uint32_t)b << bs);
}

__attribute__((sysv_abi, section(".text.entry")))
void kernel_main(BootInfo *info) {
    uint32_t rm = info->fb.red_mask;
    uint32_t gm = info->fb.green_mask;
    uint32_t bm = info->fb.blue_mask;
    volatile uint32_t *fb = (volatile uint32_t*)(uintptr_t)info->fb.framebuffer_base;
    uint32_t stride = info->fb.pixels_per_scanline;

    uint32_t cpu_count = 0;

    if (info->acpi.rsdp != 0) {
        acpi_rsdp_t *rsdp = (acpi_rsdp_t *)(uintptr_t)info->acpi.rsdp;
        acpi_rsdt_t *rsdt = (acpi_rsdt_t *)(uintptr_t)rsdp->rsdt_address;
        
        // Purple square RSDT
        uint32_t purple = pack_rgb(255, 0, 255, rm, gm, bm);
        for(int y=0; y<40; y++) for(int x=0; x<40; x++) fb[y*stride + x + 200] = purple;

        uint32_t entries = (rsdt->header.length - sizeof(acpi_sdt_header_t)) / 4;

        for (uint32_t i = 0; i < entries; i++) {
            acpi_sdt_header_t *table = (acpi_sdt_header_t *)(uintptr_t)rsdt->tables[i];

            if (table->signature[0] == 'A' && table->signature[1] == 'P' && 
                table->signature[2] == 'I' && table->signature[3] == 'C') {
                
                // Gold square for MADT
                uint32_t gold = pack_rgb(255, 215, 0, rm, gm, bm);
                for(int y=0; y<40; y++) for(int x=0; x<40; x++) fb[y*stride + x + 250] = gold;

                acpi_madt_t *madt = (acpi_madt_t *)table;
                uint8_t *ptr = (uint8_t *)madt + sizeof(acpi_madt_t);
                uint8_t *end = (uint8_t *)madt + madt->header.length;

                while (ptr < end) {
                    acpi_madt_entry_t *entry = (acpi_madt_entry_t *)ptr;
                    if (entry->type == 0) { // Local APIC
                        madt_entry_lapic_t *lapic = (madt_entry_lapic_t *)ptr;
                        if (lapic->flags & 1) cpu_count++;
                    }
                    ptr += entry->length;
                }
            }
        }
    }

    // Blue squares for each CPU
    uint32_t blue = pack_rgb(0, 0, 255, rm, gm, bm);
    for (uint32_t i = 0; i < cpu_count; i++) {
        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < 20; x++) {
                fb[(450 * stride) + (40 + i * 30) + x + (y * stride)] = blue;
            }
        }
    }

    for (;;) { __asm__ __volatile__("hlt"); }
}