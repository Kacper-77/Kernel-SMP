#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include <stdint.h>
#include <stddef.h>

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
} __attribute__((packed)) BootFramebuffer;

typedef struct {
    void    *memory_map;
    uint64_t memory_map_size;
    uint64_t descriptor_size;
    uint32_t descriptor_version;
    uint32_t _pad0;  // Filler
} __attribute__((packed)) BootMemoryMap;

typedef struct {
    void *rsdp;
} __attribute__((packed)) BootAcpi;

typedef struct {
    void    *kernel_entry;
    void    *kernel_base;
    uint64_t kernel_size;
} __attribute__((packed)) BootKernel;

typedef struct {
    void* ramdisk_addr;
    uint64_t ramdisk_size;
    
    BootFramebuffer fb;
    BootMemoryMap   mmap;
    BootAcpi        acpi;
    BootKernel      kernel;
} __attribute__((packed)) BootInfo;

#endif
