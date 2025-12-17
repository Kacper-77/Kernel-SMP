#include <boot.h>
#include <memory_map.h>

EFI_STATUS init_memory_map(BootMemoryMap *mmap, UINTN *out_map_key) {
    EFI_STATUS status;
    UINTN memory_map_size = 0;
    EFI_MEMORY_DESCRIPTOR *memory_map = NULL;
    UINTN map_key = 0;
    UINTN descriptor_size = 0;
    UINT32 descriptor_version = 0;

    //
    // First GetMemoryMap() call:
    // UEFI returns EFI_BUFFER_TOO_SMALL and tells us how big the buffer must be.
    // This is intentional: firmware memory layout is dynamic and cannot be predicted.
    //
    status = gBS->GetMemoryMap(
        &memory_map_size,
        memory_map,
        &map_key,
        &descriptor_size,
        &descriptor_version
    );

    if (status != EFI_BUFFER_TOO_SMALL) {
        boot_panic(L"[MM] First GetMemoryMap did not return EFI_BUFFER_TOO_SMALL");
    }
    
    //
    // Diagnostic logging — useful for debugging firmware behavior
    // Only after first GetMemoryMap()!!!
    //
    boot_log(L"[MM] Memory map initialized");

    CHAR16 buf[128];
    UINTN entry_count = memory_map_size / descriptor_size;
    UnicodeSPrint(buf, sizeof(buf), L"[MM] Entries: %lu", entry_count);
    boot_log(buf);

    //
    // Add extra space:
    // UEFI may allocate memory *between* the two GetMemoryMap() calls.
    // If we don't add slack, the second call may fail with BUFFER_TOO_SMALL again.
    //
    memory_map_size += 2 * descriptor_size;

    //
    // Allocate the buffer for the actual memory map.
    // EfiLoaderData ensures the buffer is tracked as loader-owned memory.
    //
    status = gBS->AllocatePool(EfiLoaderData, memory_map_size, (VOID**)&memory_map);
    if (EFI_ERROR(status) || memory_map == NULL) {
        boot_panic(L"[MM] Failed to allocate memory for MemoryMap");
    }

    //
    // Second GetMemoryMap():
    // This time UEFI writes the actual memory map into our buffer.
    // The returned MapKey must be passed to ExitBootServices().
    //
    status = gBS->GetMemoryMap(
        &memory_map_size,
        memory_map,
        &map_key,
        &descriptor_size,
        &descriptor_version
    );

    if (EFI_ERROR(status)) {
        boot_panic(L"[MM] Second GetMemoryMap failed");
    }

    //
    // Store the raw memory map for the kernel.
    // The kernel will parse this to build its physical memory manager.
    //
    mmap->memory_map         = memory_map;
    mmap->memory_map_size    = memory_map_size;
    mmap->descriptor_size    = descriptor_size;
    mmap->descriptor_version = descriptor_version;

    // MapKey is not part of BootMemoryMap because only the bootloader needs it.
    if (out_map_key) {
        *out_map_key = map_key;
    }

    return EFI_SUCCESS;
}
