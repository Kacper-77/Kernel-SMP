#ifndef EFI_DESC_H
#define EFI_DESC_H

//
// Only to avoid problems with UEFI lib
//

typedef struct {
    uint32_t type;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t num_pages;
    uint64_t attribute;
} EFI_MEMORY_DESCRIPTOR;

#endif
