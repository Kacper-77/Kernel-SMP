#include <boot.h>
#include <memory_map.h>

//
// Fetch the final UEFI memory map and exit Boot Services.
// This must be called *after* all allocations, file I/O, GOP setup,
// ACPI scanning, and kernel loading.
//
// After ExitBootServices(), Boot Services become invalid and must not be used.
//
EFI_STATUS final_memory_map_and_exit(BootMemoryMap *mmap, EFI_HANDLE ImageHandle) {
    EFI_STATUS status;

    UINTN memory_map_size = 0;
    EFI_MEMORY_DESCRIPTOR *memory_map = NULL;
    UINTN map_key = 0;
    UINTN descriptor_size = 0;
    UINT32 descriptor_version = 0;

    //
    // First GetMemoryMap() call:
    // - memory_map must be NULL
    // - UEFI returns EFI_BUFFER_TOO_SMALL and tells us required buffer size
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
    // Add slack:
    // UEFI may allocate memory between the first and second GetMemoryMap() calls.
    // Without extra space, the second call may fail again.
    //
    memory_map_size += 2 * descriptor_size;

    //
    // Allocate buffer for the actual memory map.
    // EfiLoaderData ensures the kernel can reclaim this memory later.
    //
    status = gBS->AllocatePool(EfiLoaderData, memory_map_size, (VOID**)&memory_map);
    if (EFI_ERROR(status) || memory_map == NULL) {
        boot_panic(L"[MM] Failed to allocate memory for MemoryMap");
    }

retry:
    //
    // Second GetMemoryMap():
    // - UEFI writes the actual memory map into our buffer
    // - map_key must be passed to ExitBootServices()
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
    // ExitBootServices():
    // - may fail if memory changed between GetMemoryMap() and ExitBootServices()
    // - if so, we must retry the entire sequence
    //
    status = gBS->ExitBootServices(ImageHandle, map_key);
    if (EFI_ERROR(status)) {
        //
        // Retry: memory map changed -> fetch again and try ExitBootServices again
        //
        status = gBS->GetMemoryMap(
            &memory_map_size,
            memory_map,
            &map_key,
            &descriptor_size,
            &descriptor_version
        );

        if (EFI_ERROR(status)) {
            boot_panic(L"[MM] GetMemoryMap retry failed");
        }

        status = gBS->ExitBootServices(ImageHandle, map_key);
        if (EFI_ERROR(status)) {
            boot_panic(L"[MM] ExitBootServices failed");
        }
    }

    //
    // Boot Services are now gone.
    // Only Runtime Services remain valid.
    //
    mmap->memory_map         = memory_map;
    mmap->memory_map_size    = memory_map_size;
    mmap->descriptor_size    = descriptor_size;
    mmap->descriptor_version = descriptor_version;

    return EFI_SUCCESS;
}
