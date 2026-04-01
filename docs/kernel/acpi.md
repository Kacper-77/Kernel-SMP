# Advanced Configuration and Power Interface (ACPI) Parser

## Overview
The `acpi` module provides the fundamental logic required to discover and parse ACPI System Description Tables. This is a critical component for modern x86-64 hardware initialization, as it allows the kernel to identify hardware topology, specifically the Multiple APIC Description Table (MADT) required for Symmetric Multiprocessing (SMP) and Advanced Programmable Interrupt Controller (APIC) configuration.

## Design Decisions

1. Dual-Standard Support (RSDT/XSDT)
The parser is designed to be compatible with both ACPI 1.0 and ACPI 2.0+ specifications.
- XSDT Priority: If the Root System Description Pointer (RSDP) revision is 2.0 or higher and a 64-bit Extended System Description Table (XSDT) address is provided, the driver prioritizes it. This ensures compatibility with modern UEFI-based systems and 64-bit physical addressing.
- RSDT Fallback: For older hardware or ACPI 1.0 implementations, the driver falls back to the 32-bit Root System Description Table (RSDT).

2. Physical to Virtual Mapping
Since ACPI tables are provided by the firmware in physical memory, the parser utilizes the kernel's `phys_to_virt` translation layer. This assumes that the ACPI memory regions have been identity-mapped or mapped into the Higher Half during the early stages of Virtual Memory Manager (VMM) initialization.

3. Signature-Based Discovery
Instead of hardcoding table offsets, the driver implements a generic `acpi_find_table` function. This function iterates through the pointer array within the Root/Extended tables and performs a 4-byte signature comparison (e.g., "APIC" for MADT, "FACP" for FADT). This modular approach makes it easy to add support for new ACPI tables in the future.

## Technical Details

Data Structures:
- acpi_sdt_header_t: The standard 36-byte header shared by all System Description Tables, containing the signature, length, and checksum.
- acpi_rsdp_t: The initial structure found in memory (usually via UEFI configuration tables) that points to the rest of the ACPI stack.
- MADT (Multiple APIC Description Table): Specifically defined to extract Local APIC addresses and processor IDs, which is essential for routing interrupts to specific cores in an SMP environment.

Alignment and Packing:
- All ACPI structures use `__attribute__((packed))` to prevent the compiler from adding padding, as these structures must match the exact byte-layout defined by the ACPI specification.

Table Iteration Logic:
- For XSDT, the entry size is 8 bytes (64-bit pointers).
- For RSDT, the entry size is 4 bytes (32-bit pointers).
- The number of entries is dynamically calculated using the formula: `(header.length - sizeof(header)) / entry_size`.

## Future Improvements
- Checksum Validation: Implement a function to verify the `checksum` byte of each SDT header to ensure data integrity before parsing.
- FADT/DSDT Parsing: Add support for the Fixed ACPI Description Table and Differentiated System Description Table to enable power management features like system shutdown and sleep states.
- AML Interpreter: In the long term, integrate a minimal AML (ACPI Machine Language) interpreter or use a library like `uACPI` / `ACPICA` to handle complex hardware configuration and power events.
- HPET Discovery: Add logic to find the High Precision Event Timer table for improved timing accuracy.
