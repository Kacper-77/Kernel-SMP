#ifndef ACPI_H
#define ACPI_H

#include <Uefi.h>
#include <boot_info.h>

// Locates the ACPI RSDP using the UEFI configuration table.
// On success, stores the pointer in BootAcpi and returns EFI_SUCCESS.
EFI_STATUS init_acpi(BootAcpi *acpi);

#endif
