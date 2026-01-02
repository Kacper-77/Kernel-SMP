#ifndef CPU_H
#define CPU_H

typedef struct cpu_context {
    uint64_t cpu_id;             // Local ID
    uint64_t lapic_id;           // Physical APIC ID
    uint64_t kernel_stack;       // Kernel stack for CPU
    struct cpu_context* self;
    
    // MORE LATER
} __attribute__((packed)) cpu_context_t;

static inline cpu_context_t* get_cpu() {
    cpu_context_t* ptr;
    // 'q' - 64-bit quadword
    __asm__ volatile ("movq %%gs:24, %0" : "=r"(ptr)); 
    return ptr;
}

// Helper MSR_GS_BASE = 0xC0000101
static inline void cpu_init_context(cpu_context_t* ctx) {
    uint32_t low = (uint32_t)(uintptr_t)ctx;
    uint32_t high = (uint32_t)((uintptr_t)ctx >> 32);
    __asm__ volatile ("wrmsr" : : "a"(low), "d"(high), "c"(0xC0000101) : "memory");
}

#endif
