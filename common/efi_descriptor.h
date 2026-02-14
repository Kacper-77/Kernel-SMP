#ifndef EFI_DESC_H
#define EFI_DESC_H

typedef struct {
    uint32_t type;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t num_pages;
    uint64_t attribute;
} EFI_MEMORY_DESCRIPTOR;

#define EFI_CONVENTIONAL_MEMORY     7
#define EFI_LOADER_CODE             1
#define EFI_LOADER_DATA             2
#define EFI_BOOT_SERVICES_CODE      3
#define EFI_BOOT_SERVICES_DATA      4

#endif
