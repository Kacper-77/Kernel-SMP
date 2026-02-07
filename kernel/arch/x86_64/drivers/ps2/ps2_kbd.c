#include <ps2_kbd.h>
#include <io.h>
#include <i8042.h>
#include <serial.h>
#include <spinlock.h>

static spinlock_t kbd_lock_ = { .lock = 0, .owner = -1, .recursion = 0 };

// Handler section
static int is_caps     = 0;
static int is_break    = 0;
static int is_shifted  = 0;
static int is_extended = 0;

// Buffer section
#define KBD_BUF_SIZE 256
static char kbd_buffer[KBD_BUF_SIZE];
static volatile int kbd_head = 0; 
static volatile int kbd_tail = 0;

//
// Main keyboard handler, extracting chars from PS2 sets
// by passed scancodes
//
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
            kbd_push_char((char)ascii);
        }
    }

    is_extended = 0; // Ensure extended state is cleared after processing
}// Logic: Shift reverses the effect of Caps Lock for letters

//
// Pushing char to Circular Buffer - nessesary for Ring 3
//
void kbd_push_char(char c) {
    uint64_t f = save_interrupts_and_cli();
    spin_lock(&kbd_lock_);

    int next = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next != kbd_tail) {
        kbd_buffer[kbd_head] = c;
        kbd_head = next;
    }

    spin_unlock(&kbd_lock_);
    restore_interrupts(f);
}

//
// Pops char from Circular Buffer
//
char kbd_pop_char() {
    uint64_t f = save_interrupts_and_cli();
    spin_lock(&kbd_lock_);

    char c = 0;
    if (kbd_head != kbd_tail) {
        c = kbd_buffer[kbd_tail];
        kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    }

    spin_unlock(&kbd_lock_);
    restore_interrupts(f);
    return c;
}
