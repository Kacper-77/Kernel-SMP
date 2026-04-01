# I/O Advanced Programmable Interrupt Controller (I/O APIC) Driver

## Overview
The `ioapic` module provides the driver for the I/O Advanced Programmable Interrupt Controller. While the Local APIC (LAPIC) handles interrupts internal to the CPU, the I/O APIC is responsible for collecting external hardware interrupts (from peripherals such as the keyboard, timers, or PCI devices) and routing them to the appropriate Local APICs across the SMP system.

## Design Decisions

1. Indirect Register Access
The I/O APIC uses an indirect addressing mechanism via two 32-bit Memory-Mapped I/O (MMIO) registers: `IOREGSEL` (Index) and `IOWIN` (Data).
- Encapsulation: The driver abstracts this into `ioapic_read` and `ioapic_write` functions, ensuring that every register access correctly sets the index before performing the data transaction.

2. Redirection Table Management
The core of the I/O APIC is the Redirection Table (offsets 0x10 to 0x3F).
- Dynamic IRQ Routing: The `ioapic_set_irq` function configures how a specific hardware pin (e.g., IRQ 1 for the keyboard) is mapped to a software vector in the IDT and which CPU (via its APIC ID) should receive it.
- Unmasking: By clearing Bit 16 of the redirection entry, the driver explicitly enables the delivery of the interrupt only when the corresponding handler is ready.

3. Virtual Memory and Caching (PCD Flag)
During initialization, the I/O APIC base address is mapped into the Higher Half.
- Page-level Cache Disable: Crucially, the mapping uses the `PCD` flag to ensure that the CPU does not cache accesses to the I/O APIC registers. This guarantees that every read/write operation reflects the current hardware state, preventing synchronization issues.

4. Safe Initialization and Sanitization
The `ioapic_init` function performs a full sanitization of the hardware state:
- Capability Discovery: It reads the `VER` register to determine the `max_entries` (the number of redirection pins available on the specific chipset).
- Default Masking: All redirection entries are initially set to 0x10000 (masked), preventing spurious or unhandled interrupts from firing before the kernel is fully initialized.

## Technical Details

Register Mapping:
- IOREGSEL (0x00): Index selection register.
- IOWIN (0x04): Data window register.

Redirection Entry Structure (64-bit):
- Low 32 bits: Contains the interrupt vector and flags (Delivery Mode, Destination Mode, Mask).
- High 32 bits: Contains the destination (APIC ID of the target processor).



## Future Improvements
- Multi-I/O APIC Support: Extend the driver to support systems with multiple I/O APIC chips by maintaining a list of base addresses and their respective Global System Interrupt ranges.
- Destination Modes: Implement Logical Destination Mode to allow interrupts to be delivered to a group of processors (e.g., "lowest priority" delivery) to balance the interrupt load.
- PCI MSI/MSI-X Support: Coordinate with the PCI driver to manage Message Signaled Interrupts, which bypass the I/O APIC by writing directly to the Local APIC's memory-mapped registers.
