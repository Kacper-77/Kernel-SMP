#ifndef I8042_H
#define I8042_H

#include <stdint.h>

// PORTS
#define PS2_DATA_PORT    0x60
#define PS2_STATUS_REG   0x64
#define PS2_COMMAND_REG  0x64

// REGISTER FLAGS
#define PS2_STATUS_OUTPUT_FULL 0x01 // Bit 0: Data waits for CPU
#define PS2_STATUS_INPUT_FULL  0x02 // Bit 1: Cannot write
#define PS2_STATUS_SYSTEM      0x04 // Bit 2: System flag
#define PS2_STATUS_CMD_DATA    0x08 // Bit 3: 0 = data, 1 = command
#define PS2_STATUS_TIMEOUT     0x40 // Bit 6: Timeout error
#define PS2_STATUS_PARITY      0x80 // Bit 7: Parity error

// CONTROLLER COMMANDS
#define I8042_CMD_READ_CONFIG  0x20
#define I8042_CMD_WRITE_CONFIG 0x60
#define I8042_CMD_SELF_TEST    0xAA
#define I8042_CMD_DISABLE_P1   0xAD
#define I8042_CMD_ENABLE_P1    0xAE
#define I8042_CMD_DISABLE_P2   0xA7
#define I8042_CMD_ENABLE_P2    0xA8

// HELPERS
void i8042_init();
void i8042_wait_write();
void i8042_wait_read();
void i8042_write_command(uint8_t cmd);
void i8042_write_data(uint8_t data);
uint8_t i8042_read_data();

#endif
