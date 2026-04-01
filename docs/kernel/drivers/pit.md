# Programmable Interval Timer (PIT) Driver

## Overview
The `pit` module provides a low-level driver for the Intel 8254 Programmable Interval Timer. Unlike the high-level system timer (LAPIC), this driver is specifically designed for high-precision, short-duration busy-waiting and hardware calibration. It operates by utilizing PIT Channel 0 in Mode 0 (Interrupt on Terminal Count) to provide reliable timing independent of the kernel's interrupt subsystem.

## Design Decisions

1. Use for Calibration (Busy-Waiting)
The PIT in this kernel is primarily used as a reference clock to calibrate the Local APIC (LAPIC) timers on each CPU core. Because the LAPIC frequency varies between hardware models, the PIT—which runs at a fixed base frequency of 1.193182 MHz—provides the "gold standard" required to measure how many LAPIC ticks occur in a known millisecond interval.

2. Mode 0 - Interrupt on Terminal Count
The driver configures the PIT in Mode 0. In this mode, the output pin stays low while the counter is counting down and transitions to high exactly when the counter reaches zero. This allows for a deterministic way to measure time without needing to enable IRQ0, which is useful during early boot stages or SMP initialization where interrupts might be masked or unrouted.

3. Read-Back Command Polling
Instead of latching the counter value (which is slow and prone to race conditions in a multi-core environment if not locked), the driver uses the 8254 Read-Back Command (0xE2).
- Status Latching: The Read-Back command latches the status byte of Channel 0.
- Output Bit Monitoring: The driver polls Bit 7 (Output Pin Status) of the status byte. This is a robust way to detect the end of the countdown across different hardware implementations.

4. Hardware Backoff
Within the `pit_wait_calibration` loop, the `pause` instruction is used. This prevents the CPU from consuming excessive power and helps the processor's out-of-order execution logic handle the busy-wait loop more efficiently.

## Technical Details

Constants:
- PIT_BASE_FREQUENCY: 1193182 Hz (derived from the 14.31818 MHz master clock divided by 12).
- PIT_COMMAND_PORT: 0x43 (Used for Mode/Command configuration).
- PIT_CHANNEL0_PORT: 0x40 (Used for loading the 16-bit divisor).

Command Format (0x30):
- Channel: 00 (Channel 0)
- Access: 11 (Load Low-byte then High-byte)
- Mode: 000 (Mode 0: Interrupt on Terminal Count)
- Format: 0 (16-bit Binary)

Read-Back Command (0xE2):
- Type: 11 (Read-Back)
- Latch: 10 (Latch status only, do not latch count)
- Select: 001 (Channel 0)

## Future Improvements
- Multi-Channel Support: Implement Channel 2 support (connected to the PC Speaker) for audio generation.
- Dynamic Frequency Scaling: Add a wrapper to calculate PIT ticks based on a requested microsecond value to simplify calibration logic.
- IRQ0 Integration: Optionally allow the PIT to drive the legacy timer interrupt if the APIC is unavailable or disabled on specific hardware.
