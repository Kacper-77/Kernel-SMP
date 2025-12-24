#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include <stdint.h>
#include <stddef.h>

//
// RAW Boot Info for Kernel
//

typedef struct {
    uint64_t framebuffer_size;
    void    *framebuffer_base;
    uint32_t width;
    uint32_t height;
    uint32_t pixels_per_scanline;
    uint32_t format;
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t reserved_mask;
} BootFramebuffer;

typedef struct {
    void    *memory_map;
    uint64_t memory_map_size;
    uint64_t descriptor_size;
    uint32_t descriptor_version;
} BootMemoryMap;

typedef struct {
    void *rsdp;
} BootAcpi;

typedef struct {
    void    *kernel_entry;
    void    *kernel_base;
    uint64_t kernel_size;
} BootKernel;

typedef struct {
    BootFramebuffer fb;
    BootMemoryMap   mmap;
    BootAcpi        acpi;
    BootKernel      kernel;
} BootInfo;

//
// PMM struct
//
typedef struct {
    uint32_t type;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t num_pages;
    uint64_t attribute;
} EFI_MEMORY_DESCRIPTOR;

#endif
