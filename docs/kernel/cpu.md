# CPU Management and Per-CPU Context

## Overview
The `cpu` module responsible for low-level processor initialization and state management. It implements the "Per-CPU" storage pattern using the `GS` segment register, handles the transition to 64-bit Long Mode features (SSE/AVX, NXE, PAE), and configures the hardware-accelerated `SYSCALL` mechanism.

## Design Decisions

1. Per-CPU Context via GS-Base
To enable efficient SMP (Symmetric Multiprocessing), the kernel needs a way to access core-specific data (like the current task, scheduler queues, or TSS) without using global locks.
- Mechanism: The driver utilizes the `IA32_GS_BASE` Model Specific Register (MSR). By loading the address of a `cpu_context_t` structure into this MSR, the kernel can access per-core data using the `%gs` prefix (e.g., `movq %gs:0, %rax`).
- Atomic Access: The `get_cpu()` inline function provides a zero-overhead way to retrieve the local context, which is fundamental for thread-safe scheduler operations.

2. Fast System Calls
Bypasses the legacy `int 0x80` interrupt-based system calls in favor of the modern `SYSCALL` instruction.
- Performance: `SYSCALL` eliminates the overhead of IDT lookup and gate checking.
- Configuration: The driver sets up `IA32_LSTAR` (entry point), `IA32_STAR` (segment selectors for Ring 0/3), and `IA32_FMASK` (RFLAGS masking). This ensures that interrupts are atomically disabled upon entering the kernel.

3. Floating Point and Vector Extensions (SSE)
To support modern applications, the kernel explicitly enables SSE (Streaming SIMD Extensions).
- CR4 Configuration: Sets `OSFXSR` and `OSXMMEXCPT` bits to enable `fxsave`/`fxrstor` and unmasked SIMD floating-point exceptions.
- CR0 Cleanup: Clears the `EM` (Emulation) bit to ensure the CPU uses the hardware FPU/XMM units instead of trapping to software.

4. Multi-Level Runqueue (Scheduler State)
The `cpu_context_t` includes a built-in 4-priority level runqueue. This design localizes the scheduler's state to each core, reducing cache contention and allowing for more efficient task distribution in 32-core environments.

## Technical Details

Critical MSRs used:
- 0xC0000080 (EFER): Extended Feature Enable Register (NX bit, Syscall enable).
- 0xC0000081 (STAR): Legacy Segment Base/Limit.
- 0xC0000082 (LSTAR): Long Mode Target Address for SYSCALL.
- 0xC0000101 (GS_BASE): Base address for the `GS` segment.

Memory Management Features:
- NXE (No-Execute Enable): Enabled via EFER to allow for non-executable stack and heap pages, enhancing system security.
- WP (Write Protect): Managed in `CR0` to prevent the kernel from accidentally overwriting read-only pages, even with supervisor privileges.

Hardware Stack Management:
- Each CPU core is allocated a dedicated 16KB kernel stack (`pmm_alloc_frames(4)`). The address is stored in the core's `TSS.rsp0` to ensure a safe stack switch occurs during Ring 3 -> Ring 0 transitions.

## Future Improvements
- AVX/AVX-512 Support: Implement `XCR0` and `xsave` logic to support 256-bit and 512-bit registers.
- Per-CPU Memory Allocator: Integrate a local slab allocator to further reduce lock contention on the global PMM.
- IBPB/STIBP Mitigation: Add support for speculative execution mitigations (Spectre/Meltdown) by toggling relevant bits in MSRs during context switches.
