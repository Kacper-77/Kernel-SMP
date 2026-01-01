#include <pmm.h>
#include <vmm.h>
#include <efi_descriptor.h>

uint8_t* bitmap = NULL;
uint64_t bitmap_size = 0;
static uint64_t last_checked_index = 0; // Optimization for searching

//
// Atomic
//
static pmm_lock_t pmm_lock_ = {0};

static void pmm_lock() {
    while (__sync_lock_test_and_set(&pmm_lock_.lock, 1)) {
        __asm__ volatile("pause");
    }
}

static void pmm_unlock() {
    __sync_lock_release(&pmm_lock_.lock);
}

//
// Helpers
//
void pmm_set_frame(uint64_t frame_addr) {
    uint64_t frame_index = frame_addr / PAGE_SIZE;
    bitmap[frame_index / 8] |= (1 << (frame_index % 8));
}

void pmm_unset_frame(uint64_t frame_addr) {
    uint64_t frame_index = frame_addr / PAGE_SIZE;
    bitmap[frame_index / 8] &= ~(1 << (frame_index % 8));
}

int pmm_is_frame_set(uint64_t frame_addr) {
    uint64_t frame_index = frame_addr / PAGE_SIZE;
    return (bitmap[frame_index / 8] & (1 << (frame_index % 8))) != 0;
}

//
// INIT
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
                if (addr >= 0x100000) pmm_unset_frame(desc->physical_start + (j * PAGE_SIZE));
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
    
    // Safety check
    last_checked_index = 0;
}

//
// ALLOC and FREE
//
void* pmm_alloc_frame() {
    if (bitmap == NULL) return NULL;
    
    // Lock before start
    pmm_lock();

    for (uint64_t i = last_checked_index; i < bitmap_size; i++) {
        if (bitmap[i] == 0xFF) continue;

        for (int b = 0; b < 8; b++) {
            if (!(bitmap[i] & (1 << b))) {
                uint64_t frame_index = i * 8 + b;
                uint64_t frame_addr = frame_index * PAGE_SIZE;

                if (frame_addr == 0) continue; 

                uint64_t idx = frame_index / 8;
                bitmap[idx] |= (1 << (frame_index % 8));
                
                last_checked_index = i;
                pmm_unlock();  // Unlock
                return (void*)frame_addr;
            }
        }
    }
    pmm_unlock(); // Unlock even frame haven't found
    return NULL; 
}

void pmm_free_frame(void* frame) {
    if (!frame) return;

    pmm_lock();

    uint64_t addr = (uint64_t)frame;
    pmm_unset_frame(addr);

    // After freeing, update last_checked_index to look here next time
    uint64_t frame_index = (addr / PAGE_SIZE) / 8;
    if (frame_index < last_checked_index) {
        last_checked_index = frame_index;
    }
    
    pmm_unlock();
}
