#ifndef VMM_H
#define VMM_H

#include <boot_info.h>

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

// Page Table Entry Flags
#define PTE_PRESENT     (1ULL << 0)  // Page is in memory
#define PTE_WRITABLE    (1ULL << 1)  // Page can be written to
#define PTE_USER        (1ULL << 2)  // Page accessible by user-mode
#define PTE_PWT         (1ULL << 3)  // Page-level write-through
#define PTE_PCD         (1ULL << 4)  // Page-level cache disable
#define PTE_ACCESSED    (1ULL << 5)  // Set by CPU when page is read
#define PTE_DIRTY       (1ULL << 6)  // Set by CPU when page is written
#define PTE_HUGE        (1ULL << 7)  // Large page (PS bit)
#define PTE_GLOBAL      (1ULL << 8)  // Global page (not flushed from TLB)
#define PTE_NX          (1ULL << 63) // No-execute bit (requires EFER.NXE)

typedef uint64_t pt_entry;

typedef struct {
    pt_entry entries[512];
} page_table_t;

// Get indices from virtual address
#define PML4_IDX(addr) (((addr) >> 39) & 0x1FF)
#define PDPT_IDX(addr) (((addr) >> 30) & 0x1FF)
#define PD_IDX(addr)   (((addr) >> 21) & 0x1FF)
#define PT_IDX(addr)   (((addr) >> 12) & 0x1FF)

// Align an address down to page boundary
#define PAGE_ALIGN_DOWN(addr) ((addr) & ~0xFFFULL)

// Align an address up to page boundary
#define PAGE_ALIGN_UP(addr) (((addr) + 0xFFF) & ~0xFFFULL)

void vmm_init(BootInfo* bi);
void vmm_enable_pat();
uintptr_t phys_to_virt(uintptr_t phys);
uintptr_t vmm_virtual_to_physical(page_table_t* pml4, uintptr_t virt);
void vmm_map(page_table_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t flags);
void* vmm_map_device(page_table_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t size);
void vmm_map_range(page_table_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t size, uint64_t flags);
void vmm_unmap(page_table_t* pml4, uintptr_t virt);
void vmm_unmap_range(page_table_t* pml4, uintptr_t virt, uint64_t size);

page_table_t* vmm_get_pml4();
uintptr_t vmm_get_pml4_phys();

#endif
