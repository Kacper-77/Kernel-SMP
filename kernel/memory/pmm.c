#include <pmm.h>
#include <serial.h>

static uint8_t* bitmap;
static uint64_t bitmap_size;
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
    uint64_t total_ram = 0;
    BootMemoryMap* mmap = &boot_info->mmap;
    uint64_t entries = mmap->memory_map_size / mmap->descriptor_size;

    // First pass: Find the highest physical address to determine bitmap size
    for (uint64_t i = 0; i < entries; i++) {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)mmap->memory_map + (i * mmap->descriptor_size));
        uint64_t end_addr = desc->physical_start + (desc->num_pages * PAGE_SIZE);
        if (end_addr > highest_addr) highest_addr = end_addr;
        if (desc->type == EFI_CONVENTIONAL_MEMORY) total_ram += desc->num_pages * PAGE_SIZE;
    }

    // bitmap_size = (number of pages) / 8 bits per byte
    bitmap_size = (highest_addr / PAGE_SIZE) / 8;

    // Second pass: Find a free spot for the bitmap itself
    for (uint64_t i = 0; i < entries; i++) {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)mmap->memory_map + (i * mmap->descriptor_size));
        if (desc->type == EFI_CONVENTIONAL_MEMORY && (desc->num_pages * PAGE_SIZE) >= bitmap_size) {
            bitmap = (uint8_t*)desc->physical_start;
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

    // Lock frames occupied by the bitmap itself and the first 1MB (legacy)
    for (uint64_t i = 0; i < (bitmap_size / PAGE_SIZE + 1); i++) {
        pmm_set_frame((uint64_t)bitmap + (i * PAGE_SIZE));
    }
    
    // Lock first 1MB to protect BIOS/UEFI structures and IVT
    for (uint64_t i = 0; i < 0x100000; i += PAGE_SIZE) {
        pmm_set_frame(i);
    }

    kprint("PMM: Initialized. Total usable RAM: "); kprint_hex(total_ram / 1024 / 1024); kprint(" MiB\n");
}

//
// ALLOC and FREE
//
void* pmm_alloc_frame() {
    // Search through the bitmap byte by byte
    for (uint64_t i = last_checked_index; i < bitmap_size; i++) {
        // If the byte is 0xFF, all 8 frames are taken, skip it
        if (bitmap[i] == 0xFF) continue;

        // Found a byte with at least one free bit (0)
        for (int b = 0; b < 8; b++) {
            if (!(bitmap[i] & (1 << b))) {
                uint64_t frame_index = i * 8 + b;
                uint64_t frame_addr = frame_index * PAGE_SIZE;

                pmm_set_frame(frame_addr);
                last_checked_index = i; // Save progress for next time
                return (void*)frame_addr;
            }
        }
    }

    // No free memory left!
    // In a real OS, this would trigger swapping or OOM killer
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
