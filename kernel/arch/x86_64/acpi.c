#include <vmm.h>
#include <acpi.h>
#include <serial.h>
#include <std_funcs.h>

#include <stdbool.h>

void* acpi_find_table(acpi_rsdp_t* rsdp, char* signature) {
    bool use_xsdt = (rsdp->revision >= 2 && rsdp->xsdt_address != 0);
    
    if (use_xsdt) {
        acpi_xsdt_t* xsdt = (acpi_xsdt_t*)phys_to_virt(rsdp->xsdt_address);
        uint32_t entries = (xsdt->header.length - sizeof(acpi_sdt_header_t)) / 8;

        for (uint32_t i = 0; i < entries; i++) {
            acpi_sdt_header_t* table = (acpi_sdt_header_t*)phys_to_virt(xsdt->tables[i]);
            if (memcmp(table->signature, signature, 4) == 0) return table;
        }
    } else {
        acpi_rsdt_t* rsdt = (acpi_rsdt_t*)phys_to_virt(rsdp->rsdt_address);
        uint32_t entries = (rsdt->header.length - sizeof(acpi_sdt_header_t)) / 4;

        for (uint32_t i = 0; i < entries; i++) {
            acpi_sdt_header_t* table = (acpi_sdt_header_t*)phys_to_virt(rsdt->tables[i]);
            if (memcmp(table->signature, signature, 4) == 0) return table;
        }
    }
    return NULL;
}
