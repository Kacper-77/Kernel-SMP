#include <pmm.h>
#include <vmm.h>
#include <cpu.h>
#include <spinlock.h>
#include <efi_descriptor.h>

#define HHDM_OFFSET 0xFFFF800000000000

uint8_t* bitmap = NULL;
uint64_t bitmap_size = 0;

//
// Atomic
//
static spinlock_t pmm_lock_ = { .lock = 0, .owner = -1, .recursion = 0 };

//
// Updates the physical memory bitmap by marking a specific 4KB frame as used (1).
// This is a low-level helper used during allocation and manual frame reservations.
//
void pmm_set_frame(uint64_t frame_addr) {
    uint64_t frame_index = frame_addr / PAGE_SIZE;
    bitmap[frame_index / 8] |= (1 << (frame_index % 8));
}

//
// Marks a specific 4KB frame as free (0) in the physical memory bitmap.
//
void pmm_unset_frame(uint64_t frame_addr) {
    uint64_t frame_index = frame_addr / PAGE_SIZE;
    bitmap[frame_index / 8] &= ~(1 << (frame_index % 8));
}

//
// Checks specific frame.
//
int pmm_is_frame_set(uint64_t frame_addr) {
    uint64_t frame_index = frame_addr / PAGE_SIZE;
    return (bitmap[frame_index / 8] & (1 << (frame_index % 8))) != 0;
}

//
// Initializes the Physical Memory Manager using the EFI Memory Map.
// Performs three passes:
// 1. Determines the highest physical address to size the bitmap.
// 2. Finds a large enough contiguous free region to store the bitmap.
// 3. Marks usable RAM (Conventional Memory) as free, while protecting 
//    the first 1MB, the kernel, and the bitmap itself.
//
void pmm_init(BootInfo* boot_info) {
    uint64_t highest_addr = 0;
    BootMemoryMap* mmap = &boot_info->mmap;
    uint64_t entries = mmap->memory_map_size / mmap->descriptor_size;

    // First pass: Find the highest physical address to determine bitmap size
    for (uint64_t i = 0; i < entries; i++) {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)mmap->memory_map + (i * mmap->descriptor_size));
        uint64_t end_addr = desc->physical_start + (desc->num_pages * PAGE_SIZE);
        if (end_addr > highest_addr) highest_addr = end_addr;
    }

    // bitmap_size = (number of pages) / 8 bits per byte
    bitmap_size = (highest_addr / PAGE_SIZE) / 8;

    // Second pass: Find a free spot for the bitmap itself
    for (uint64_t i = 0; i < entries; i++) {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)mmap->memory_map + (i * mmap->descriptor_size));
        if (desc->type == EFI_CONVENTIONAL_MEMORY && (desc->num_pages * PAGE_SIZE) >= bitmap_size) {
            bitmap = (uint8_t*)PAGE_ALIGN_UP(desc->physical_start);
            // Initialize bitmap: set all frames as "used" (1) by default
            for(uint64_t j = 0; j < bitmap_size; j++) bitmap[j] = 0xFF;
            break;
        }
    }

    // Third pass: Unlock only Conventional Memory frames in the bitmap
    for (uint64_t i = 0; i < entries; i++) {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)mmap->memory_map + (i * mmap->descriptor_size));
        if (desc->type == EFI_CONVENTIONAL_MEMORY || desc->type == 4) {
            for (uint64_t j = 0; j < desc->num_pages; j++) {
                uint64_t addr = desc->physical_start + (j * PAGE_SIZE);
                if (addr >= 0x100000) pmm_unset_frame(addr);
            }
        }
    }
    
    // Lock first 1MB (BIOS/UEFI)
    for (uint64_t i = 0; i < 0x100000; i += PAGE_SIZE) pmm_set_frame(i);

    // Lock KERNEL
    uint64_t kernel_start = (uint64_t)boot_info->kernel.kernel_base;
    uint64_t kernel_size = boot_info->kernel.kernel_size;
    for (uint64_t addr = kernel_start; addr < kernel_start + kernel_size + PAGE_SIZE; addr += PAGE_SIZE) {
        pmm_set_frame(addr);
    }

    // Lock BITMAP
    uint64_t bitmap_addr = (uint64_t)bitmap;
    for (uint64_t i = 0; i < (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE; i++) {
        pmm_set_frame(bitmap_addr + (i * PAGE_SIZE));
    }
}

//
// Allocates a single 4KB physical frame.
// Uses a per-CPU hint (pmm_last_index) to speed up the search and reduce
// lock contention in SMP environments. Returns a 4KB-aligned physical address.
//
void* pmm_alloc_frame() {
    if (bitmap == NULL) return NULL;
    
    spin_lock(&pmm_lock_);

    // Get current CPU context
    cpu_context_t* cpu = get_cpu();
    // If not we use default value
    uint64_t start_index = cpu ? cpu->pmm_last_index : 0;

    for (uint64_t i = start_index; i < bitmap_size; i++) {
        if (bitmap[i] == 0xFF) continue;

        for (int b = 0; b < 8; b++) {
            if (!(bitmap[i] & (1 << b))) {
                uint64_t frame_index = i * 8 + b;
                uint64_t frame_addr = frame_index * PAGE_SIZE;

                if (frame_addr == 0) continue; 

                bitmap[i] |= (1 << b);
                
                if (cpu) cpu->pmm_last_index = i;

                spin_unlock(&pmm_lock_);
                return (void*)frame_addr;
            }
        }
    }
    spin_unlock(&pmm_lock_); // Unlock even if frame wasn't found
    return NULL; 
}

//
// Allocates multiple contiguous physical frames.
// Used primarily for large data structures like kernel stacks or initial paging tables.
// Search is performed globally from the start of the bitmap.
//
void* pmm_alloc_frames(size_t count) {
    if (bitmap == NULL || count == 0) return NULL;

    // Idiot proof
    if (count == 1) return pmm_alloc_frame();

    spin_lock(&pmm_lock_);

    uint64_t total_bits = bitmap_size * 8;

    for (uint64_t i = 0; i < total_bits - count; i++) {
        uint64_t free_found = 0;

        for (uint64_t j = 0; j < count; j++) {
            uint64_t bit_idx = i + j;
            if (!(bitmap[bit_idx / 8] & (1 << (bit_idx % 8)))) {
                free_found++;
            } else {
                break;
            }
        }

        if (free_found == count) {
            for (uint64_t j = 0; j < count; j++) {
                uint64_t bit_idx = i + j;
                bitmap[bit_idx / 8] |= (1 << (bit_idx % 8));
            }

            uint64_t phys_addr = i * PAGE_SIZE;
            spin_unlock(&pmm_lock_);
            return (void*)phys_addr;
        }
    }

    spin_unlock(&pmm_lock_);
    return NULL;
}

void pmm_free_frame(void* frame) {
if (!frame) return;

    spin_lock(&pmm_lock_);

    uint64_t addr = (uint64_t)frame;
    pmm_unset_frame(addr);

    cpu_context_t* cpu = get_cpu();
    uint64_t frame_index = (addr / PAGE_SIZE) / 8;
    
    if (cpu && frame_index < cpu->pmm_last_index) {
        cpu->pmm_last_index = frame_index;
    }
    spin_unlock(&pmm_lock_);
}

//
// Adjusts the bitmap pointer to use the Higher Half Direct Map (HHDM) address.
// This must be called after the virtual memory manager is initialized and 
// CR3 is loaded, allowing the PMM to be accessible in virtual address space.
//
void pmm_move_to_high_half() {
    if (bitmap != NULL && (uintptr_t)bitmap < HHDM_OFFSET) {
        bitmap = (uint8_t*)phys_to_virt((uintptr_t)bitmap);
    }
}