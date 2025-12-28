#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include <Uefi.h>
#include <Library/UefiLib.h>

//
// Framebuffer information passed to the kernel.
// Used for early graphics, text output, and debugging.
//
typedef struct {
    UINTN   framebuffer_size;      // total size in bytes
    VOID   *framebuffer_base;    // Physical address of the framebuffer
    UINT32  width;               // Horizontal resolution in pixels
    UINT32  height;              // Vertical resolution in pixels
    UINT32  pixels_per_scanline; // Number of pixels per row (pitch)

    // Pixel format and masks to avoid guessing in the kernel
    UINT32  format;                // EFI_GRAPHICS_PIXEL_FORMAT
    UINT32  red_mask;
    UINT32  green_mask;
    UINT32  blue_mask;
    UINT32  reserved_mask;
} BootFramebuffer;

//
// Raw UEFI memory map passed to the kernel.
// Kernel uses this to build its own physical memory manager.
//
typedef struct {
    VOID   *memory_map;         // Pointer to the memory map buffer
    UINTN   memory_map_size;    // Total size of the memory map
    UINTN   descriptor_size;    // Size of each EFI_MEMORY_DESCRIPTOR
    UINT32  descriptor_version; // Version of the descriptor format
} BootMemoryMap;

//
// ACPI root pointer (RSDP).
// Kernel uses this to locate ACPI tables (MADT, FADT, etc.)
// required for APIC and SMP initialization.
//
typedef struct {
    VOID   *rsdp;  // Physical address of the RSDP structure
} BootAcpi;

//
// Kernel image information.
// Set by the bootloader after loading the kernel from disk.
//
typedef struct {
    VOID   *kernel_entry;       // Entry point address of the kernel
    VOID   *kernel_base;        // Base address where the kernel was loaded
    UINTN   kernel_size;        // Size of the loaded kernel image
} BootKernel;

//
// Main structure passed to the kernel at startup.
// Contains all essential boot-time information.
//
typedef struct {
    BootFramebuffer fb;      // Framebuffer info
    BootMemoryMap   mmap;    // UEFI memory map
    BootAcpi        acpi;    // ACPI RSDP pointer
    BootKernel      kernel;  // Kernel load info
} BootInfo;

#endif
