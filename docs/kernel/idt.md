# Interrupt Descriptor Table (IDT) and Exception Handling

## Overview
The `idt` module is the central communication hub between the hardware and the kernel. It defines how the CPU responds to asynchronous events (Hardware Interrupts), synchronous software errors (Exceptions), and Inter-Processor Interrupts (IPIs). This implementation is optimized for x86-64 Long Mode and is fully compatible with Symmetric Multiprocessing (SMP).

## Design Decisions

1. Low-Level Assembly Stubs (idt_stubs.asm)
To maintain a consistent stack state, every one of the 256 possible interrupt vectors has a dedicated assembly entry point.
- Uniform Stack Frame: Since the CPU only pushes an error code for specific exceptions, the macros `isr_no_err_stub` and `isr_err_stub` ensure that the stack always contains a vector number and an error code (dummy or real) before jumping to the `common_stub`.
- Context Preservation: The `common_stub` saves all General Purpose Registers (GPRs) into an `interrupt_frame_t` structure, providing the C-level handler with the full state of the CPU at the moment of the interrupt.

2. SWAPGS Mechanism for Ring Transition
The interrupt handler includes critical logic for managing the `GS` base register:
- Privilege Detection: By testing the `CS` selector pushed by the CPU (`test qword [rsp + 144], 3`), the stub determines if the interrupt occurred in User Mode (Ring 3).
- Context Switching: If coming from User Mode, `swapgs` is executed to switch from the User GS base to the Kernel GS base (where Per-CPU data is stored). It is swapped back before `iretq` to restore the user environment.

3. Centralized Dispatcher (interrupt_dispatch)
Instead of 256 separate C functions, a single `interrupt_dispatch` function handles the logic:
- Exception Handling: Vectors 0-31 are routed to the `exception_handler` for register dumping and system panics.
- IRQ Routing: Standard hardware interrupts (e.g., Timer at 32, Keyboard at 33) are routed to their respective drivers.
- EOI (End of Interrupt): The Local APIC is notified of interrupt completion via `lapic_send_eoi()` for all non-exception vectors.

## Technical Details

Data Structures:
- idt_entry: 16-byte descriptor containing the 64-bit Interrupt Service Routine (ISR) address, segment selector, and attributes (Gate Type, DPL, Present bit).
- interrupt_frame_t: A packed structure representing the state of the stack. It includes registers pushed by hardware (RIP, CS, RFLAGS, RSP, SS) and those pushed by the assembly stubs.

Interrupt Attributes (0x8E):
- Type: 0xE (64-bit Interrupt Gate). Interrupts are disabled automatically upon entry to prevent nested interrupt complexity.
- DPL: 0 (Kernel only).
- P: 1 (Present).

Vector Allocation:
- 0-31: CPU Exceptions (Faults, Traps, Aborts).
- 32: System Timer (LAPIC Timer).
- 33: PS/2 Keyboard.
- 0xFE (IPI_VECTOR_HALT): Inter-processor halt signal.
- 0xFF (IPI_VECTOR_TEST): Inter-processor TLB flush or synchronization signal.



## Technical Assembly Logic (common_stub)
1. Pushes GPRs (RAX through R15).
2. Conditional `swapgs` based on previous privilege level.
3. Passes the stack pointer (`RSP`) as a pointer to the `interrupt_frame_t` in `RDI` (System V ABI).
4. `call interrupt_dispatch`.
5. Updates `RSP` with the return value of the dispatcher (allowing for seamless task switching/context replacement).
6. Restores registers and performs `iretq`.

## Future Improvements
- IST (Interrupt Stack Table): Assign specific IST indices to critical exceptions (like Double Fault or Machine Check) to ensure a valid stack even during kernel stack corruption.
- User-Mode Interrupts: Update DPL for specific gates (e.g., `int 0x80` or `int 3`) to allow userspace-triggered software interrupts.
- Spurious Interrupt Handling: Add logic to detect and silently ignore spurious interrupts from the APIC.
