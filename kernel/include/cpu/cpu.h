#ifndef CPU_H
#define CPU_H

#include <stdint.h>

extern cpu_context_t* cpu_table[32];

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
    gdt_tss_entry_t tss_entry;
} __attribute__((packed)) cpu_gdt_t;

typedef struct cpu_context {
    struct cpu_context* self;       // offset 0
    uint64_t cpu_id;                // offset 8
    uint64_t lapic_id;              // offset 16
    uint64_t user_rsp;              // offset 24
    uint64_t kernel_stack;          // offset 32
    
    // TSS for each core
    tss_t tss;                      
    cpu_gdt_t gdt;               
    
    // Descriptor LGDT
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdt_ptr;

    // Runqueue 
    spinlock_t rq_lock;             // CPU exclusive lock
    struct task* rq_head;
    struct task* rq_tail;           
    uint64_t rq_count;       
    
    // Scheduler
    struct task* current_task;
    struct task* idle_task;
    
    uint64_t pmm_last_index;
    uint32_t lapic_ticks_per_ms;
} __attribute__((packed)) cpu_context_t;

// Initialization of BSP and SYSCALLS
void cpu_init_bsp();
void cpu_init_syscalls();

static inline cpu_context_t* get_cpu() {
    cpu_context_t* ptr;

    __asm__ volatile ("movq %%gs:0, %0" : "=r"(ptr)); 
    return ptr;
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

//
// CR3 READ AND WRITE
//
static inline uint64_t read_cr3(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline void write_cr3(uint64_t val) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(val) : "memory");
}

//
// MSR READ AND WRITE
//
static inline uint64_t read_msr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile (
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(msr)
    );
    return ((uint64_t)high << 32) | low;
}

static inline void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    __asm__ volatile (
        "wrmsr"
        :
        : "a"(low), "d"(high), "c"(msr)
        : "memory"
    );
}

static inline void cpu_init_context(cpu_context_t* ctx) {
    ctx->self = ctx;
    ctx->rq_head = NULL;
    ctx->rq_tail = NULL;
    ctx->rq_count = 0;
    ctx->rq_lock = (spinlock_t){0};
    uint64_t addr = (uintptr_t)ctx;
    
    // MSR_GS_BASE (0xC0000101)
    write_msr(0xC0000101, addr);

    // MSR_KERNEL_GS_BASE (0xC0000102)
    write_msr(0xC0000102, addr);
}

static inline void cpu_register_context(cpu_context_t* ctx) {
    cpu_table[ctx->cpu_id] = ctx;
}

static inline cpu_context_t* get_cpu_by_id(uint64_t id) {
    if (id >= 32) return NULL;
    return cpu_table[id];
}

#endif
