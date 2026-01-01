#include <vmm.h>
#include <pmm.h>
#include <efi_descriptor.h>

static page_table_t* kernel_pml4 = NULL;

// Symbols from the new linker script
extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];

// Physical address where the kernel is loaded
#define KERNEL_PHYS_BASE 0x2000000
// Virtual address where the kernel starts
#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000
// Default addr mask
#define VMM_ADDR_MASK 0x000000FFFFFFF000ULL

#define HHDM_OFFSET 0xFFFF800000000000

void* memset(void* dest, int ch, size_t count) {
    unsigned char* ptr = (unsigned char*)dest;
    while (count--) {
        *ptr++ = (unsigned char)ch;
    }
    return dest;
}

//
// Atomic
//
static vmm_lock_t vmm_lock_ = {0};

static void vmm_lock() {
    while (__sync_lock_test_and_set(&vmm_lock_.lock, 1)) {
        __asm__ volatile("pause");
    }
}

static void vmm_unlock() {
    __sync_lock_release(&vmm_lock_.lock);
}

//
// Invalidates a single page in the TLB (Translation Lookaside Buffer).
// Must be called after changing an existing mapping to ensure the CPU
// doesn't use stale data from its internal cache.
//
static inline void vmm_invlpg(void* addr) {
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

//
// Configures the Page Attribute Table (PAT) MSR to define 
// memory caching types. We set PAT4 to Write-Combining (WC) 
// for high-performance framebuffer access.
//
static void vmm_enable_pat() {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(0x277));

    uint64_t pat = ((uint64_t)high << 32) | low;

    // Set PAT4 (Write-Combining)
    pat &= ~(0xFFULL << 32);      
    pat |= (0x01ULL << 32);       

    low = (uint32_t)pat;
    high = (uint32_t)(pat >> 32);

    __asm__ volatile("wrmsr" : : "a"(low), "d"(high), "c"(0x277));
}

//
// Maps a virtual page to a physical frame.
// If intermediate tables don't exist, they are allocated using PMM.
//
void vmm_map(page_table_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t flags) {
    vmm_lock();  // Lock

    uint64_t pml4_i = PML4_IDX(virt);
    uint64_t pdpt_i = PDPT_IDX(virt);
    uint64_t pd_i   = PD_IDX(virt);
    uint64_t pt_i   = PT_IDX(virt);

    // Level 4 -> Level 3
    if (!(pml4->entries[pml4_i] & PTE_PRESENT)) {
        uint64_t new_table = (uint64_t)pmm_alloc_frame();
        memset((void*)new_table, 0, PAGE_SIZE);
        pml4->entries[pml4_i] = (new_table & VMM_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE;
    }
    page_table_t* pdpt = (page_table_t*)(pml4->entries[pml4_i] & VMM_ADDR_MASK);

    // Level 3 -> Level 2
    if (!(pdpt->entries[pdpt_i] & PTE_PRESENT)) {
        uint64_t new_table = (uint64_t)pmm_alloc_frame();
        memset((void*)new_table, 0, PAGE_SIZE);
        pdpt->entries[pdpt_i] = (new_table & VMM_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE;
    }
    page_table_t* pd = (page_table_t*)(pdpt->entries[pdpt_i] & VMM_ADDR_MASK);

    // Level 2 -> Level 1
    if (!(pd->entries[pd_i] & PTE_PRESENT)) {
        uint64_t new_table = (uint64_t)pmm_alloc_frame();
        memset((void*)new_table, 0, PAGE_SIZE);
        pd->entries[pd_i] = (new_table & VMM_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE;
    }
    page_table_t* pt = (page_table_t*)(pd->entries[pd_i] & VMM_ADDR_MASK);

    // Map final leaf entry
    pt->entries[pt_i] = (phys & VMM_ADDR_MASK) | flags | PTE_PRESENT;

    vmm_unlock();  // Unlock
}

// 2MB
void vmm_map_huge(page_table_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t flags) {
    vmm_lock();

    uint64_t pml4_i = PML4_IDX(virt);
    uint64_t pdpt_i = PDPT_IDX(virt);
    uint64_t pd_i   = PD_IDX(virt);

    if (!(pml4->entries[pml4_i] & PTE_PRESENT)) {
        uint64_t new_table = (uint64_t)pmm_alloc_frame();
        memset((void*)new_table, 0, PAGE_SIZE);
        pml4->entries[pml4_i] = (new_table & VMM_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE;
    }
    page_table_t* pdpt = (page_table_t*)(pml4->entries[pml4_i] & VMM_ADDR_MASK);

    if (!(pdpt->entries[pdpt_i] & PTE_PRESENT)) {
        uint64_t new_table = (uint64_t)pmm_alloc_frame();
        memset((void*)new_table, 0, PAGE_SIZE);
        pdpt->entries[pdpt_i] = (new_table & VMM_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE;
    }
    page_table_t* pd = (page_table_t*)(pdpt->entries[pdpt_i] & VMM_ADDR_MASK);

    pd->entries[pd_i] = (phys & VMM_ADDR_MASK) | flags | 0x81; 

    vmm_unlock();
}

//
// Maps Memory Mapped I/O (MMIO) devices.
// Uses Write-Combining (via PAT4) for performance, while 
// ensuring No-Execute (NX) and Writable permissions.
//
void* vmm_map_device(page_table_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t size) {
    uintptr_t phys_aligned = PAGE_ALIGN_DOWN(phys);
    uintptr_t offset = phys - phys_aligned;
    uint64_t full_size = PAGE_ALIGN_UP(size + offset);

    // Use PAT bit (bit 7) to trigger Write-Combining (WC) via PAT4 entry
    uint64_t flags = PTE_PRESENT | PTE_WRITABLE | PTE_NX | (1ULL << 7);

    vmm_map_range(pml4, virt, phys_aligned, full_size, flags);

    return (void*)(virt + offset);
}

//
// Performs a "Page Walk" to translate a virtual address to its physical counterpart.
// Returns the physical address or 0 if the page is not mapped.
//
uintptr_t vmm_virtual_to_physical(page_table_t* pml4, uintptr_t virt) {
    // Traverse the 4-level page table hierarchy
    if (!(pml4->entries[PML4_IDX(virt)] & PTE_PRESENT)) return 0;
    page_table_t* pdpt = (page_table_t*)(pml4->entries[PML4_IDX(virt)] & VMM_ADDR_MASK);

    if (!(pdpt->entries[PDPT_IDX(virt)] & PTE_PRESENT)) return 0;
    page_table_t* pd = (page_table_t*)(pdpt->entries[PDPT_IDX(virt)] & VMM_ADDR_MASK);

    if (!(pd->entries[PD_IDX(virt)] & PTE_PRESENT)) return 0;
    page_table_t* pt = (page_table_t*)(pd->entries[PD_IDX(virt)] & VMM_ADDR_MASK);

    if (!(pt->entries[PT_IDX(virt)] & PTE_PRESENT)) return 0;

    // Physical address = (address from PT entry) + (12-bit offset from virtual address)
    return (pt->entries[PT_IDX(virt)] & VMM_ADDR_MASK) + (virt & 0xFFF);
}

//
// Transform PA -> VA
//
uintptr_t phys_to_virt(uintptr_t phys) {
    return phys + HHDM_OFFSET;
}

//
// Maps a contiguous range of virtual pages to a contiguous range of physical frames.
// Automatically handles TLB invalidation for the entire range.
//
void vmm_map_range(page_table_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t size, uint64_t flags) {
    uint64_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (uint64_t i = 0; i < num_pages; i++) {
        uintptr_t v_addr = virt + (i * PAGE_SIZE);
        uintptr_t p_addr = phys + (i * PAGE_SIZE);
        
        vmm_map(pml4, v_addr, p_addr, flags);
        vmm_invlpg((void*)v_addr);
    }
}

//
// Initializes paging by creating a new PML4, identity mapping critical regions,
// and performing the switch via CR3 and segment reloading.
//
void vmm_init(BootInfo* bi) {
    vmm_enable_pat();
    // CRITICAL: Use a local pointer (stored on the stack) instead of the global 'kernel_pml4'.
    // The global variable is located in the .bss section at a high virtual address (0xFFFFFFFF...),
    // which will cause a Page Fault if accessed before paging is enabled.
    page_table_t* local_pml4 = (page_table_t*)pmm_alloc_frame();
    memset(local_pml4, 0, PAGE_SIZE);

    // 1. BOOTSTRAP IDENTITY MAPPING (First 512 MiB)
    // Necessary to keep the current execution flow alive when CR3 is swapped.
    // Maps physical 0x0 -> virtual 0x0.
    // NOTE: will be changed
    for (uint64_t addr = 0; addr < 0x20000000; addr += PAGE_SIZE) {
        vmm_map(local_pml4, addr, addr, PTE_PRESENT | PTE_WRITABLE);
    }

    // 2.  DIRECT PHYSICAL MAPPING (HHDM)
    uint64_t desc_size = bi->mmap.descriptor_size;
    uint64_t total_map_size = bi->mmap.memory_map_size;
    uint8_t* map_ptr = (uint8_t*)bi->mmap.memory_map;

    for (uint64_t offset = 0; offset < total_map_size; offset += desc_size) {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)(map_ptr + offset);

        if (desc->type != 5 && desc->num_pages > 0) {
            uint64_t phys_start = desc->physical_start;
            uint64_t size = desc->num_pages * 4096;

            vmm_map_range(local_pml4, phys_start + HHDM_OFFSET, phys_start, size, PTE_PRESENT | PTE_WRITABLE);
        }
    }

    // 3. HIGHER HALF KERNEL MAPPING
    size_t kernel_size = (size_t)(_kernel_end - _kernel_start);
    vmm_map_range(local_pml4, KERNEL_VIRT_BASE, KERNEL_PHYS_BASE, kernel_size,
                PTE_PRESENT | PTE_WRITABLE);


    // 4. MAP THE STACK
    // Ensures the current stack remains valid after the switch.
    uint64_t current_rsp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(current_rsp));
    uint64_t stack_page = PAGE_ALIGN_DOWN(current_rsp);
    int stack_pages = 16;

    for (int i = 0; i < stack_pages; i++) {
        uintptr_t phys_addr = stack_page - (i * PAGE_SIZE);
        uintptr_t virt_addr = phys_to_virt(phys_addr);
        vmm_map(local_pml4, virt_addr, phys_addr, PTE_PRESENT | PTE_WRITABLE);
    }

    // 5. MAP THE FRAMEBUFFER
    uintptr_t fb_phys = (uintptr_t)bi->fb.framebuffer_base;
    uintptr_t fb_virt = phys_to_virt(fb_phys); 
    vmm_map_device(local_pml4, fb_virt, fb_phys, bi->fb.framebuffer_size);
    
    bi->fb.framebuffer_base = (void*)fb_virt;

    // 6. MAP THE BOOTINFO
    // Ensure the BootInfo structure is accessible after the address space switch.
    uintptr_t bi_phys = (uintptr_t)bi;
    uintptr_t bi_virt = phys_to_virt((uintptr_t)bi);
    vmm_map_range(
        local_pml4, 
        PAGE_ALIGN_DOWN(bi_virt), 
        PAGE_ALIGN_DOWN(bi_phys), 
        sizeof(BootInfo), 
        PTE_PRESENT
    );

    // 7. ENABLE NXE IN EFER MSR (No-Execute Enable)
    __asm__ volatile(
        "mov $0xC0000080, %%ecx\n"
        "rdmsr\n"
        "or $0x800, %%eax\n"
        "wrmsr\n"
        ::: "ecx", "eax", "edx"
    );

    // 8. ENABLE PAE IN CR4
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 5);
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));

    // 9. TEMPORARILY DISABLE WP IN CR0
    // This allows the kernel to initialize tables without immediate protection faults.
    uint64_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 16);
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

    // 10. LOAD CR3 AND SWITCH STACK TO HIGHER HALF
    __asm__ volatile(
        "mov %0, %%cr3\n\t"                  
        "movabs $0xFFFF800000000000, %%rax\n\t"
        "add %%rax, %%rsp\n\t"               
        "add %%rax, %%rbp"                   
        : 
        : "r"(local_pml4) 
        : "rax", "memory"
    );

    // 11. SAFE TO ACCESS GLOBAL VARIABLES NOW
    kernel_pml4 = local_pml4;
}

// Getter
page_table_t* vmm_get_pml4() {
    return kernel_pml4;
}
