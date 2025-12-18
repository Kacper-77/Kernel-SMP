#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include <Uefi.h>
#include <boot_info.h>

// Initializes the UEFI memory map and stores it in BootMemoryMap.
// Returns the MapKey required for ExitBootServices().
EFI_STATUS final_memory_map_and_exit(BootMemoryMap *mmap, EFI_HANDLE ImageHandle);

#endif
