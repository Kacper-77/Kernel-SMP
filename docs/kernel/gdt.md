# Global Descriptor Table (GDT) and Task State Segment (TSS)

## Overview
The `gdt` module is responsible for defining the memory segmentation model and processor privilege levels for the x86-64 architecture. Although x86-64 uses a flat memory model where segmentation is largely bypassed, the GDT remains a fundamental requirement for switching between Kernel Mode (Ring 0) and User Mode (Ring 3), as well as for defining the Task State Segment (TSS) used for hardware-assisted stack switching during interrupts.

## Design Decisions

1. Per-CPU GDT and TSS
In a Symmetric Multiprocessing (SMP) environment, each CPU core must have its own GDT and TSS.
- Isolation: Each core manages its own Task Register (TR) and stack pointers.
- TSS Stack Switching: The `tss_t` structure within each `cpu_context_t` holds the `rsp0` value (Kernel Stack), which the CPU automatically loads when a privilege transition occurs (e.g., an interrupt firing while in Ring 3).

2. Flat Segmentation (64-bit Long Mode)
The driver configures a flat model where all segments base at `0x00` and have a limit of `0xFFFFFFFFFFFFFFFF`.
- Kernel Segments: Offset 0x08 (Code) and 0x10 (Data).
- User Segments: Offset 0x1B (Code, RPL 3) and 0x23 (Data, RPL 3).
- Granularity/Access: Entries are configured with the Long Mode bit (L-bit) set in the granularity field, signaling 64-bit code execution.

3. Manual Segment Flushing
Loading the GDT via the `lgdt` instruction is not sufficient to update the processor's internal shadowed registers.
- Data Segments: Manually reloaded (DS, ES, SS) via `mov` instructions.
- Code Segment (CS): Updated using a "Far Return" (`retfq`) trick. Since there is no direct `mov cs, ax` instruction, the driver pushes the new selector and the return address onto the stack and performs a far return to force a CS update.

## Technical Details

Memory Layout:
- GDT Entry 0: Null Descriptor (Required).
- GDT Entry 1: Kernel Code (Access 0x9A).
- GDT Entry 2: Kernel Data (Access 0x92).
- GDT Entry 3: User Code (Access 0xFA).
- GDT Entry 4: User Data (Access 0xF2).
- GDT Entry 5: TSS Descriptor (System Segment, 16 bytes/Double Entry).

TSS Configuration:
- Unlike standard segments, the TSS descriptor in 64-bit mode is expanded to 16 bytes to accommodate a 64-bit base address.
- The `ltr` (Load Task Register) instruction is used to load the TSS selector (0x28) into the Task Register, pointing the hardware to the current core's TSS.



## Technical Assembly Logic (gdt_flush)
The assembly helper ensures the CPU state is synchronized with the new table:
1. `lgdt [rdi]`: Loads the GDTR with the limit and base.
2. `mov ds, ax ...`: Flushes data selectors.
3. `push 0x08` / `push rdi` / `retfq`: Atomically switches the Code Segment and resumes execution at the return address.

## Future Improvements
- IST (Interrupt Stack Table): Implement IST support within the TSS to provide dedicated, clean stacks for critical exceptions like Double Faults (#DF) or Machine Checks (#MC).
- FS/GS Base: Utilize `wrfsbase` and `wrgsbase` (if supported by CPUID) or MSRs (`IA32_GS_BASE`) for more efficient Thread Local Storage (TLS) and Per-CPU data management, replacing the legacy GDT-based FS/GS approach.
- User Mode Hardening: Verify that all segment limits and access rights strictly adhere to the principle of least privilege.
