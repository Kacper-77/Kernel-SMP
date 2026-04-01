# Local APIC (LAPIC) and Inter-Processor Interrupts (IPI)

## Overview
The `apic` module implements the driver for the Local Advanced Programmable Interrupt Controller (LAPIC). It is a foundational component for Symmetric Multiprocessing (SMP), responsible for local interrupt delivery, high-precision timing, and cross-core communication via Inter-Processor Interrupts (IPIs).

## Design Decisions

1. Memory-Mapped I/O Interface
The LAPIC is accessed via memory-mapped registers (default base 0xFEE00000). The driver utilizes volatile pointers to ensure that every read and write operation bypasses the CPU cache and directly interacts with the hardware registers. 

2. Cross-Core Calibration (PIT-to-LAPIC)
Since the LAPIC timer frequency is tied to the processor's bus speed (which varies across hardware), the driver performs a dynamic calibration:
- Reference Clock: The legacy Programmable Interval Timer (PIT) is used as a reliable time base.
- The 5ms Window: The Bootstrap Processor (BSP) measures how many LAPIC ticks occur during a 5ms PIT countdown.
- Global Synchronization: Once the BSP calculates the frequency, it is stored in `global_ticks_per_ms`. Application Processors (APs) wait for this value to be non-zero, ensuring all 32 cores share a synchronized time-keeping constant.

3. Inter-Processor Communication (IPI)
The driver provides robust mechanisms for cores to communicate using the Interrupt Command Register (ICR):
- Unicast IPI: Sends an interrupt to a specific core using its LAPIC ID.
- Broadcast IPI: Sends an interrupt to all other cores simultaneously (e.g., for TLB shootdowns).
- Delivery Status Monitoring: The `lapic_wait_for_delivery` function polls the "Send Pending" bit with a safety timeout to prevent kernel hangs if the APIC bus becomes unresponsive.

4. Comprehensive Register Mapping
The driver includes a complete set of LAPIC register offsets (e.g., LVT Thermal, Performance Counters, IRR). While only a subset (Timer, SVR, ICR, EOI) is currently used for core kernel logic, the infrastructure is fully prepared for advanced features such as hardware error reporting and thermal throttling interrupts.

## Technical Details

Initialization Logic:
- SVR (Spurious Interrupt Vector Register): Enables the APIC unit and sets the spurious vector to 0xFF.
- LVT (Local Vector Table): Configures LINT0/LINT1 as masked by default to avoid legacy PIC interference.
- Timer: Configured in Periodic Mode (Bit 17 of LVT Timer Register).

IPI Vector Assignments:
- IPI_VECTOR_HALT (0xFE): Emergency system-wide shutdown.
- IPI_VECTOR_TEST (0xFD): Synchronization and TLB flush signaling.

Memory Barriers:
- The driver uses a memory fence (`mfence`) after calibration. This ensures that the calculated frequency is globally visible to all other APs before they proced with their local timer initialization.

## Future Improvements
- x2APIC Support: Implement support for x2APIC mode using MSRs to support systems with >255 logical processors.
- TSC-Deadline Mode: Utilize the TSC-Deadline timer mode for even higher precision on supported Intel CPUs.
- Advanced Error Handling: Implement ISRs for the LAPIC Error Status Register (ESR) to log hardware-level delivery failures.
- Performance Monitoring: Utilize the LVT Performance Counter Register for kernel profiling.
