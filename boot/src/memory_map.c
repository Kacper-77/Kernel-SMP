#include <Uefi.h>

#include <boot.h>
#include <memory_map.h>

EFI_STATUS final_memory_map_and_exit(BootMemoryMap *mmap, EFI_HANDLE ImageHandle) {
    EFI_STATUS status;

    UINTN memory_map_size = 0;
    EFI_MEMORY_DESCRIPTOR *memory_map = NULL;
    UINTN map_key = 0;
    UINTN descriptor_size = 0;
    UINT32 descriptor_version = 0;

    // First probe for required size
    status = gBS->GetMemoryMap(&memory_map_size, memory_map, &map_key, &descriptor_size, &descriptor_version);
    if (status != EFI_BUFFER_TOO_SMALL) {
        boot_panic(L"[MM] First GetMemoryMap did not return EFI_BUFFER_TOO_SMALL");
    }

    // Generous slack to tolerate allocations between calls
    memory_map_size += 4 * descriptor_size;

    status = gBS->AllocatePool(EfiLoaderData, memory_map_size, (VOID**)&memory_map);
    if (EFI_ERROR(status) || memory_map == NULL) {
        boot_panic(L"[MM] Failed to allocate memory for MemoryMap");
    }

    for (;;) {
        // Refresh the memory map
        status = gBS->GetMemoryMap(&memory_map_size, memory_map, &map_key, &descriptor_size, &descriptor_version);
        if (status == EFI_BUFFER_TOO_SMALL) {
            // Grow buffer and retry
            memory_map_size += 4 * descriptor_size;
            gBS->FreePool(memory_map);
            memory_map = NULL;
            status = gBS->AllocatePool(EfiLoaderData, memory_map_size, (VOID**)&memory_map);
            if (EFI_ERROR(status) || memory_map == NULL) {
                boot_panic(L"[MM] Re-allocate MemoryMap buffer failed");
            }
            continue;
        }
        if (EFI_ERROR(status)) {
            boot_panic(L"[MM] GetMemoryMap failed");
        }

        // Attempt to exit boot services with the current map key
        status = gBS->ExitBootServices(ImageHandle, map_key);
        if (!EFI_ERROR(status)) {
            break; // Success
        }

        // On failure (often EFI_INVALID_PARAMETER), increase slack and retry
        memory_map_size += 2 * descriptor_size;
        gBS->FreePool(memory_map);
        memory_map = NULL;

        status = gBS->AllocatePool(EfiLoaderData, memory_map_size, (VOID**)&memory_map);
        if (EFI_ERROR(status) || memory_map == NULL) {
            boot_panic(L"[MM] Re-allocate MemoryMap buffer failed");
        }
    }

    // Boot Services are now gone only Runtime Services remain valid.
    mmap->memory_map         = memory_map;
    mmap->memory_map_size    = memory_map_size;
    mmap->descriptor_size    = descriptor_size;
    mmap->descriptor_version = descriptor_version;

    return EFI_SUCCESS;
}
