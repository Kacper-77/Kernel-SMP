# Spinlock Synchronization Primitives

## Overview
The `spinlock` module provides the primary low-level synchronization mechanism for the GeminiOS kernel. It is designed to protect shared resources in a Symmetric Multiprocessing (SMP) environment where multiple CPU cores may attempt to access kernel structures simultaneously. The implementation utilizes a "Ticket Lock" algorithm to ensure fairness and prevent CPU starvation.

## Design Decisions

1. Ticket Lock Algorithm
Unlike simple binary spinlocks (test-and-set), which can lead to specific cores being "starved" of a lock under high contention, implements a Ticket Lock.
- Fairness (FIFO): Each CPU requesting a lock receives a ticket number (`__atomic_fetch_add`). The lock is granted strictly in the order of request, as the `current` counter increments. 
- Deterministic Latency: This prevents "lucky" cores from grabbing a lock repeatedly while others wait indefinitely.

2. Atomic Memory Orderings
The implementation uses GCC/Clang atomic built-ins with specific memory model constraints to ensure correctness across different CPU architectures:
- __ATOMIC_ACQUIRE: Used when loading the `current` ticket to ensure that memory operations following the lock acquisition are not reordered before the lock is actually held.
- __ATOMIC_RELEASE: Used when unlocking to ensure all previous memory writes are visible to other cores before the lock is released.
- __ATOMIC_RELAXED: Used for the ticket increment, where only atomicity is required, not strict ordering.

3. CPU Backoff and Power Efficiency
Within the acquisition loop, the driver executes the `pause` instruction.
- Pipeline Management: It signals the CPU that it is in a spin-wait loop, preventing the processor from exhausting pipeline resources and reducing power consumption.
- Memory Bus Relief: It reduces the frequency of cache coherency traffic (MESI protocol) on the system bus while waiting for a ticket change.

4. Global Lock Safety (g_lock_enabled)
The implementation includes a safety check for early boot stages. Before the SMP subsystem and per-CPU structures are initialized, spinlocks act as "no-ops". This allows the same kernel code to run during the single-core bootstrap phase without triggering null pointer offsets or uninitialized state errors.

## Technical Details

Data Structure (spinlock_t):
- ticket: The next available ticket number (incremented on request).
- current: The ticket number currently holding the lock.
- last_cpu: A debug field storing the ID of the CPU core currently holding the lock.

Core Functions:
- spin_lock: Performs an atomic fetch-and-add to get a ticket and spins until the `current` value matches.
- spin_unlock: Increments the `current` value using an atomic store with release semantics.
- spin_trylock: Attempts to acquire the lock without blocking. It uses a `compare_exchange` (CAS) operation to ensure the acquisition is atomic and valid at that specific moment.

## Future Improvements
- Recursive Spinlocks: Add support for nested locks by the same CPU core to prevent self-deadlocks in complex call graphs.
- Lock Debugging: Implement a timeout mechanism that triggers a Kernel Panic if a lock is held for an implausibly long time, helping to identify deadlocks.
- Poisoning: Fill the `last_cpu` field with specific patterns upon release to catch "use-after-unlock" bugs.
- NUMA Awareness: Optimize lock distribution for systems with multiple memory nodes to reduce cross-node cache snooping.
