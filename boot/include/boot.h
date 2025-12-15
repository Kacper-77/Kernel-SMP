#ifndef BOOT_H
#define BOOT_H

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

//
// Global pointers to UEFI system structures
// Initialized in efi_main() for easy access across the bootloader
//
extern EFI_SYSTEM_TABLE     *gST;   // Main UEFI system table (console, config tables, services)
extern EFI_BOOT_SERVICES    *gBS;   // Boot Services (memory allocation, protocols, file I/O)
extern EFI_RUNTIME_SERVICES *gRT;   // Runtime Services (NVRAM vars, time, system reset)

// Basic logging helpers
VOID boot_log(IN CHAR16 *msg);
VOID boot_panic(IN CHAR16 *msg);

#endif
