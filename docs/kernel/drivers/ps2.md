# PS/2 Keyboard Driver

## Overview
The `ps2_kbd` module implements a driver for PS/2 keyboards using Scancode Set 2. It translates raw hardware scancodes into ASCII characters and handles keyboard-specific states such as Shift, Caps Lock, and extended keys. The driver features an internal circular buffer to decouple interrupt-driven data acquisition from synchronous user-mode consumption.

## Design Decisions

1. Interrupt-Driven Input and Scheduling
The driver is designed to work within an Interrupt Service Routine (ISR). 
- Event Notification: Upon receiving and successfully translating a valid key press, the driver calls `sched_wakeup(REASON_KEYBOARD)`. This mechanism allows the scheduler to wake up tasks that are currently blocked waiting for user input, ensuring high responsiveness.
- Non-blocking I/O: Input is stored in an intermediate circular buffer, allowing the hardware to continue firing interrupts even if the consumer (e.g., a shell or user application) is not ready to process the character immediately.

2. State Machine for Scancode Set 2
The driver implements a state machine to handle the complexities of the PS/2 protocol:
- Break Codes (0xF0): Handles key release events to accurately track the state of modifier keys like Shift.
- Extended Codes (0xE0): Prepared for multi-byte scancodes used by special keys (multimedia, navigation).
- Modifier Logic: Implements a XOR-based logic for character casing (`is_shifted ^ is_caps`) to correctly determine when to use upper or lower case scancode sets.

3. Circular Buffer Implementation
To bridge the gap between the Kernel (Ring 0) and Applications (Ring 3), a thread-safe circular buffer is used:
- Producer-Consumer Model: `kbd_push_char` (producer) and `kbd_pop_char` (consumer) manage the head and tail pointers.
- Volatile Qualifiers: Head and tail pointers are marked `volatile` to prevent compiler optimizations from caching values that are modified across different execution contexts (ISR vs Thread).

4. SMP Synchronization
As with the underlying i8042 controller, the keyboard driver is fully SMP-aware:
- Global Input Lock: A spinlock (`kbd_lock_`) protects the shared state machine (modifiers) and the circular buffer pointers.
- Atomic Buffer Access: Accessing the buffer from a system call (via `kbd_pop_char`) involves disabling local interrupts and acquiring the spinlock to prevent data corruption if a keyboard interrupt occurs on the same or another core during the pop operation.

## Technical Details

Scancode Translation:
- The driver utilizes two static lookup tables (`scancode_set2_lower` and `scancode_set2_upper`) mapped to the standard PS/2 Set 2 layout.
- Special keys (Arrows, Ctrl, Alt) are mapped to values above 0x80 to distinguish them from standard ASCII while remaining representable in a single byte.

Buffer Parameters:
- Buffer Size: 256 characters (fixed-size).
- Overrun Handling: If the buffer is full, new characters are silently discarded to prevent corruption of existing data, prioritizing the oldest unread input.

## Future Improvements
- Multi-byte Extended Key Support: Fully implement the mapping for the 0xE0 prefix to support arrow keys and other navigation buttons in the shell.
- Scancode Set 1/3 Compatibility: Add auto-detection or explicit configuration for different scancode sets.
- Key Repeat Handling: Implement software-side debounce or repeat delay logic if hardware settings are insufficient.
- Multi-task Input Routing: Develop a system to route keyboard input to specific focused TTYs or tasks rather than a single global buffer.
