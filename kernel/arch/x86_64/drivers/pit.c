#include <pit.h>
#include <io.h>

//
// PIT Mode 0: Interrupt on Terminal Count.
// The output pin goes high when the count reaches zero.
//
void pit_prepare_sleep(uint16_t ticks) {
    // Command: Channel 0, Access Lobo/Hibyte, Mode 0, Binary
    // 0x30 = 00 (Channel 0) 11 (Lobo/Hibyte) 000 (Mode 0) 0 (Binary)
    outb(PIT_COMMAND_PORT, 0x30);
    
    // Write the divisor (low byte then high byte)
    outb(PIT_CHANNEL0_PORT, (uint8_t)(ticks & 0xFF));
    outb(PIT_CHANNEL0_PORT, (uint8_t)((ticks >> 8) & 0xFF));
}

//
// Uses the PIT Read-Back Command to poll the status of Channel 0.
// The Read-Back command (0xE2) allows us to check the Null Count 
// and Output Pin status without relying on IRQs.
//
void pit_wait_calibration() {
    while (1) {
        // Read-Back Command: 11 (Read-Back) 1 (Don't latch count) 0 (Latch status) 001 (Channel 0) 0 (Reserved)
        // 0xE2 = 11100010
        outb(PIT_COMMAND_PORT, 0xE2);
        
        uint8_t status = inb(PIT_CHANNEL0_PORT);
        
        // Bit 7 is the Output Pin status. 
        // In Mode 0, it becomes 1 when the counter reaches 0.
        if (status & 0x80) {
            break;
        }
        
        __asm__ volatile("pause");
    }
}
