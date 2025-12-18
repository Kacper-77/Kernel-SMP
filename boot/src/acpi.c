#include <boot.h>
#include <acpi.h>
#include <Guid/Acpi.h>
#include <Library/BaseMemoryLib.h> 

EFI_STATUS init_acpi(BootAcpi *acpi) {
    EFI_CONFIGURATION_TABLE *config_table = gST->ConfigurationTable;
    UINTN entry_count = gST->NumberOfTableEntries;

    VOID *rsdp = NULL;

    for (UINTN i = 0; i < entry_count; i++) {
        EFI_CONFIGURATION_TABLE *entry = &config_table[i];

        if (CompareGuid(&entry->VendorGuid, &gEfiAcpi20TableGuid) ||
            CompareGuid(&entry->VendorGuid, &gEfiAcpiTableGuid)) {

            rsdp = entry->VendorTable;
            break;
        }
    }

    if (!rsdp) {
        boot_panic(L"[ACPI] RSDP not found");
    }

    acpi->rsdp = rsdp;
    return EFI_SUCCESS;
}
