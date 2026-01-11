#ifndef PIT_H
#define PIT_H

#include <stdint.h>

// PIT Ports
#define PIT_CHANNEL0_PORT 0x40
#define PIT_CHANNEL1_PORT 0x41
#define PIT_CHANNEL2_PORT 0x42
#define PIT_COMMAND_PORT  0x43

// PIT Frequency
#define PIT_BASE_FREQUENCY 1193182

//
// brief Prepares the PIT to count down for a specific number of ticks.
// param ticks Number of PIT ticks to wait (max 65535).
//
void pit_prepare_sleep(uint16_t ticks);

//
// @brief Busy-waits until the PIT countdown finishes using the Read-Back command.
//
void pit_wait_calibration();

#endif
