#include <pmm.h>
#include <vmm.h>
#include <serial.h>

uint8_t* bitmap = NULL;
uint64_t bitmap_size = 0;
static uint64_t last_checked_index = 0; // Optimization for searching

// Standard UEFI Memory Types
#define EFI_CONVENTIONAL_MEMORY 7

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
        if (desc->type == EFI_CONVENTIONAL_MEMORY) {
            for (uint64_t j = 0; j < desc->num_pages; j++) {
                pmm_unset_frame(desc->physical_start + (j * PAGE_SIZE));
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
    
    // [memory] last_checked_index moved 
    last_checked_index = (kernel_start + kernel_size) / PAGE_SIZE / 8;

    kprint("PMM: Protected kernel at "); kprint_hex(kernel_start); kprint("\n");
}

//
// ALLOC and FREE
//
void* pmm_alloc_frame() {
    for (uint64_t i = last_checked_index; i < bitmap_size; i++) {
        if (bitmap[i] == 0xFF) continue;

        for (int b = 0; b < 8; b++) {
            if (!(bitmap[i] & (1 << b))) {
                uint64_t frame_index = i * 8 + b;
                uint64_t frame_addr = frame_index * PAGE_SIZE;

                // The address is 4KB aligned (it should be if PAGE_SIZE is 4096)
                if (frame_addr == 0) continue; 

                pmm_set_frame(frame_addr);
                last_checked_index = i;
                return (void*)frame_addr;
            }
        }
    }
    return NULL; 
}

void pmm_free_frame(void* frame) {
    uint64_t addr = (uint64_t)frame;
    pmm_unset_frame(addr);

    // After freeing, update last_checked_index to look here next time
    uint64_t frame_index = (addr / PAGE_SIZE) / 8;
    if (frame_index < last_checked_index) {
        last_checked_index = frame_index;
    }
}
