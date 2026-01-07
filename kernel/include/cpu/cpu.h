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
    
    uint64_t pmm_last_index;
} __attribute__((packed)) cpu_context_t;

void cpu_init_bsp();

static inline cpu_context_t* get_cpu() {
    uint32_t low, high;
    // Read GS_MSR 0xC0000101
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(0xC0000101));
    
    uint64_t base = ((uintptr_t)high << 32) | low;
    if (base < 0xFFFF800000000000ULL) {
        return (cpu_context_t*)0;
    }

    cpu_context_t* ptr;
    __asm__ volatile ("movq %%gs:0, %0" : "=r"(ptr)); 
    return ptr;
}

// Helper MSR_GS_BASE = 0xC0000101
static inline void cpu_init_context(cpu_context_t* ctx) {
    uint32_t low = (uint32_t)(uintptr_t)ctx;
    uint32_t high = (uint32_t)((uintptr_t)ctx >> 32);
    __asm__ volatile ("wrmsr" : : "a"(low), "d"(high), "c"(0xC0000101) : "memory");
}

static inline void cpu_enable_sse() {
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 5);  // PAE
    cr4 |= (1 << 9);  // OSFXSR
    cr4 |= (1 << 10); // OSXMMEXCPT
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));

    // CR0: MP (bit 1), ET (bit 4), NE (bit 5)
    uint64_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); // Clear EM
    cr0 |= (1 << 1);  // Set MP
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
}

#endif
