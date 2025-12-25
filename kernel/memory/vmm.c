#include "vmm.h"
#include "pmm.h"
#include <serial.h>

static page_table_t* kernel_pml4 = NULL;

void* memset(void* dest, int ch, size_t count) {
    unsigned char* ptr = (unsigned char*)dest;
    while (count--) {
        *ptr++ = (unsigned char)ch;
    }
    return dest;
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
// Maps a virtual page to a physical frame.
// If intermediate tables don't exist, they are allocated using PMM.
//
void vmm_map(page_table_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_i = PML4_IDX(virt);
    uint64_t pdpt_i = PDPT_IDX(virt);
    uint64_t pd_i   = PD_IDX(virt);
    uint64_t pt_i   = PT_IDX(virt);

    // Physical address mask (bits 12-51) to avoid reserved bit violations
    uint64_t addr_mask = 0x000FFFFFFFFFF000ULL;

    // Level 4 -> Level 3
    if (!(pml4->entries[pml4_i] & PTE_PRESENT)) {
        uint64_t new_table = (uint64_t)pmm_alloc_frame();
        memset((void*)new_table, 0, PAGE_SIZE);
        pml4->entries[pml4_i] = (new_table & addr_mask) | PTE_PRESENT | PTE_WRITABLE;
    }
    page_table_t* pdpt = (page_table_t*)(pml4->entries[pml4_i] & addr_mask);

    // Level 3 -> Level 2
    if (!(pdpt->entries[pdpt_i] & PTE_PRESENT)) {
        uint64_t new_table = (uint64_t)pmm_alloc_frame();
        memset((void*)new_table, 0, PAGE_SIZE);
        pdpt->entries[pdpt_i] = (new_table & addr_mask) | PTE_PRESENT | PTE_WRITABLE;
    }
    page_table_t* pd = (page_table_t*)(pdpt->entries[pdpt_i] & addr_mask);

    // Level 2 -> Level 1
    if (!(pd->entries[pd_i] & PTE_PRESENT)) {
        uint64_t new_table = (uint64_t)pmm_alloc_frame();
        memset((void*)new_table, 0, PAGE_SIZE);
        pd->entries[pd_i] = (new_table & addr_mask) | PTE_PRESENT | PTE_WRITABLE;
    }
    page_table_t* pt = (page_table_t*)(pd->entries[pd_i] & addr_mask);

    // Map final leaf entry
    pt->entries[pt_i] = (phys & addr_mask) | flags | PTE_PRESENT;
}

//
// Performs a "Page Walk" to translate a virtual address to its physical counterpart.
// Returns the physical address or 0 if the page is not mapped.
//
uint64_t vmm_virtual_to_physical(page_table_t* pml4, uint64_t virt) {
    uint64_t addr_mask = 0x000FFFFFFFFFF000ULL;

    // Traverse the 4-level page table hierarchy
    if (!(pml4->entries[PML4_IDX(virt)] & PTE_PRESENT)) return 0;
    page_table_t* pdpt = (page_table_t*)(pml4->entries[PML4_IDX(virt)] & addr_mask);

    if (!(pdpt->entries[PDPT_IDX(virt)] & PTE_PRESENT)) return 0;
    page_table_t* pd = (page_table_t*)(pdpt->entries[PDPT_IDX(virt)] & addr_mask);

    if (!(pd->entries[PD_IDX(virt)] & PTE_PRESENT)) return 0;
    page_table_t* pt = (page_table_t*)(pd->entries[PD_IDX(virt)] & addr_mask);

    if (!(pt->entries[PT_IDX(virt)] & PTE_PRESENT)) return 0;

    // Physical address = (address from PT entry) + (12-bit offset from virtual address)
    return (pt->entries[PT_IDX(virt)] & addr_mask) + (virt & 0xFFF);
}

//
// Maps a contiguous range of virtual pages to a contiguous range of physical frames.
// Automatically handles TLB invalidation for the entire range.
//
void vmm_map_range(page_table_t* pml4, uint64_t virt, uint64_t phys, uint64_t size, uint64_t flags) {
    uint64_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t v_addr = virt + (i * PAGE_SIZE);
        uint64_t p_addr = phys + (i * PAGE_SIZE);
        
        vmm_map(pml4, v_addr, p_addr, flags);
        vmm_invlpg((void*)v_addr);
    }
}

//
// Initializes paging by creating a new PML4, identity mapping critical regions,
// and performing the switch via CR3 and segment reloading.
//
void vmm_init(BootInfo* bi) {
    kernel_pml4 = (page_table_t*)pmm_alloc_frame();
    if (kernel_pml4 == NULL) {
        kprint("FATAL: PMM returned NULL for PML4 allocation!\n");
        for(;;);
    }
    memset(kernel_pml4, 0, PAGE_SIZE);

    // 1. Identity map the first 512 MiB (Kernel, IDT, GDT etc. are safe)
    for (uint64_t addr = 0; addr < 0x20000000; addr += PAGE_SIZE) {
        vmm_map(kernel_pml4, addr, addr, PTE_PRESENT | PTE_WRITABLE);
    }

    // 2. Identity Map the current Stack (16 pages below RSP)
    uint64_t current_rsp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(current_rsp));
    uint64_t stack_page = PAGE_ALIGN_DOWN(current_rsp);
    for (int i = 0; i < 16; i++) {
        uint64_t addr = stack_page - (i * PAGE_SIZE);
        vmm_map(kernel_pml4, addr, addr, PTE_PRESENT | PTE_WRITABLE);
    }

    // 3. Identity Map the Framebuffer
    uint64_t fb_start = (uint64_t)bi->fb.framebuffer_base;
    uint64_t fb_size  = bi->fb.framebuffer_size;
    vmm_map_range(kernel_pml4, fb_start, fb_start, fb_size, PTE_PRESENT | PTE_WRITABLE);

    // 4. Identity Map the BootInfo structure
    uint64_t bi_phys = (uint64_t)bi;
    vmm_map(kernel_pml4, PAGE_ALIGN_DOWN(bi_phys), PAGE_ALIGN_DOWN(bi_phys), PTE_PRESENT);

    // 5. Enable NXE in EFER MSR
    __asm__ volatile(
        "mov $0xC0000080, %%ecx\n"
        "rdmsr\n"
        "or $0x800, %%eax\n"
        "wrmsr\n"
        ::: "ecx", "eax", "edx"
    );

    // 6. Enable PAE in CR4
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 5);
    __asm__ volatile("mov %%cr4, %0" : : "r"(cr4));

    // 7. Temporarily disable Write Protect in CR0
    uint64_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 16);
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

    // 8. Load CR3 and refresh TLB
    __asm__ volatile("mov %0, %%cr3" : : "r"(kernel_pml4) : "memory");

    // 9. Reload segments to ensure GDT consistency in new address space
    // Now it's optional but can be useful in future
    __asm__ volatile (
        "pushq $0x08\n"
        "pushq $1f\n"
        "lretq\n"
        "1:\n"
        "mov $0x10, %%ax\n"
        "mov %%ax,  %%ds\n"
        "mov %%ax,  %%es\n"
        "mov %%ax,  %%ss\n"
        ::: "rax"
    );
    
    kprint("VMM: Paging enabled successfully.\n");
}
