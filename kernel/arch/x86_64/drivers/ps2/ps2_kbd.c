#include <ps2_kbd.h>
#include <io.h>
#include <i8042.h>
#include <serial.h>

static int is_caps     = 0;
static int is_break    = 0;
static int is_shifted  = 0;
static int is_extended = 0;

void ps2_keyboard_handler() {
    uint8_t scancode = inb(PS2_DATA_PORT);

    // 1. Handle prefixes (Set 2 specific)
    if (scancode == 0xE0) {
        is_extended = 1;
        return;
    }
    if (scancode == 0xF0) {
        is_break = 1;
        return;
    }

    // 2. Handle key release (BREAK)
    if (is_break) {
        is_break = 0;
        is_extended = 0; 
        
        // LShift (0x12) or RShift (0x59) released
        if (scancode == 0x12 || scancode == 0x59) is_shifted = 0;
        return;
    }

    // 3. Handle key press (MAKE)
    if (scancode == 0x12 || scancode == 0x59) {
        is_shifted = 1;
        return;
    }
    
    // Toggle Caps Lock (0x58)
    if (scancode == 0x58) {
        is_caps = !is_caps;
        return;
    }

    // 4. Map to ASCII or Special codes
    if (scancode < 0x80) {
        // Logic: Shift reverses the effect of Caps Lock for letters
        int use_upper = is_shifted ^ is_caps;
        uint8_t ascii = use_upper ? scancode_set2_upper[scancode] : scancode_set2_lower[scancode];
        
        if (ascii != 0) {
            char buf[2] = {(char)ascii, 0};
            kprint(buf);
        }
    }

    is_extended = 0; // Ensure extended state is cleared after processing
}
