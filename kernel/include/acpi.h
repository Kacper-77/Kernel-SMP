#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>

// SDT for all tables
typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

// Root System Description Pointer (RSDP)
typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;      
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) acpi_rsdp_t;

// Root System Description Table (RSDT)
typedef struct {
    acpi_sdt_header_t header;
    uint32_t tables[];
} __attribute__((packed)) acpi_rsdt_t;

typedef struct {
    acpi_sdt_header_t header;
    uint32_t local_apic_address;
    uint32_t flags;
} __attribute__((packed)) acpi_madt_t;

// MADT
typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) acpi_madt_entry_t;

// Processor Local APIC
typedef struct {
    acpi_madt_entry_t header;
    uint8_t processor_id;
    uint8_t apic_id;          
    uint32_t flags;
} __attribute__((packed)) madt_entry_lapic_t;

#endif
