#include <gdt.h>
#include <cpu.h>
#include <std_funcs.h>

#include <stdint.h>

//
// External assembly helper to flush segment registers.
// After lgdt, we must perform a far return or far jump to update CS,
// and manually reload data segments (DS, ES, SS, FS, GS).
//
extern void gdt_flush(uint64_t gdtr_ptr);

//
// SETUP FOR CPU
//
void gdt_setup_for_cpu(cpu_context_t* ctx) {
    memset(&ctx->gdt, 0, sizeof(cpu_gdt_t));

    // 1. CODE AND DATA
    ctx->gdt.entries[1].access = 0x9A;
    ctx->gdt.entries[1].granularity = 0x20;
    // KERNEL DATA
    ctx->gdt.entries[2].access = 0x92;
    ctx->gdt.entries[2].granularity = 0x00;
    // USER CODE
    ctx->gdt.entries[3].access = 0xFA;
    ctx->gdt.entries[3].granularity = 0x20;
    // USER DATA
    ctx->gdt.entries[4].access = 0xF2;
    ctx->gdt.entries[4].granularity = 0x00;

    // 2. TSS CONFIG
    uintptr_t tss_addr = (uintptr_t)&ctx->tss;
    memset(&ctx->tss, 0, sizeof(tss_t));
    ctx->tss.rsp0 = ctx->kernel_stack;
    ctx->tss.iopb_offset = sizeof(tss_t);

    ctx->gdt.tss_entry.limit_low = sizeof(tss_t) - 1;
    ctx->gdt.tss_entry.base_low = tss_addr & 0xFFFF;
    ctx->gdt.tss_entry.base_mid = (tss_addr >> 16) & 0xFF;
    ctx->gdt.tss_entry.flags1 = 0x89;  // Present, 64-bit TSS (Available)
    ctx->gdt.tss_entry.base_high_mid = (tss_addr >> 24) & 0xFF;
    ctx->gdt.tss_entry.base_high = (tss_addr >> 32) & 0xFFFFFFFF;

    // 3. GDTR SETUP
    ctx->gdt_ptr.limit = sizeof(cpu_gdt_t) - 1;
    ctx->gdt_ptr.base = (uintptr_t)&ctx->gdt;

    // 4. LOAD
    gdt_flush((uintptr_t)&ctx->gdt_ptr);

    // 5. LOAD TASK REGISTER
    __asm__ volatile ("ltr %%ax" : : "a"((uint16_t)0x28));
}
