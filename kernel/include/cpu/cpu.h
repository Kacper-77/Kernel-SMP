#ifndef CPU_H
#define CPU_H

#include <stdint.h>

typedef struct tss {
    uint32_t reserved0;
    uint64_t rsp0;      // Ring 0
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];    // Interrupt Stack Table
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed)) tss_t;

typedef struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

// Descriptor TSS
typedef struct gdt_tss_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  flags1;
    uint8_t  flags2;
    uint8_t  base_high_mid;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed)) gdt_tss_entry_t;

typedef struct {
    gdt_entry_t entries[5];         // Null, KCode, KData, UCode, UData
    gdt_tss_entry_t tss_entry;      // TSS
} __attribute__((packed)) cpu_gdt_t;

typedef struct cpu_context {
    struct cpu_context* self;       // offset 0
    uint64_t cpu_id;                // offset 8
    uint64_t lapic_id;              // offset 16
    
    // TSS for each core
    tss_t tss;                      
    cpu_gdt_t gdt;               
    
    // Descriptor LGDT
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdt_ptr;
    
    // Current thread NOTE: Scheduler later!
    void* current_thread;
    
    // Kernel Stack
    uint64_t kernel_stack;
} __attribute__((packed)) cpu_context_t;

static inline cpu_context_t* get_cpu() {
    cpu_context_t* ptr;
    // Addr from first field
    __asm__ volatile ("movq %%gs:0, %0" : "=r"(ptr)); 
    return ptr;
}

// Helper MSR_GS_BASE = 0xC0000101
static inline void cpu_init_context(cpu_context_t* ctx) {
    uint32_t low = (uint32_t)(uintptr_t)ctx;
    uint32_t high = (uint32_t)((uintptr_t)ctx >> 32);
    __asm__ volatile ("wrmsr" : : "a"(low), "d"(high), "c"(0xC0000101) : "memory");
}

#endif
