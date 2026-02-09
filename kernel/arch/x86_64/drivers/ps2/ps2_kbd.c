#include <ps2_kbd.h>
#include <io.h>
#include <i8042.h>
#include <serial.h>
#include <spinlock.h>

static spinlock_t kbd_lock_ = { .ticket = 0, .current = 0, .last_cpu = -1 };

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
    spin_lock(&kbd_lock_);
    
    uint8_t scancode = inb(PS2_DATA_PORT);

    if (scancode == 0xE0) {
        is_extended = 1;
    } else if (scancode == 0xF0) {
        is_break = 1;
    } else if (is_break) {
        is_break = 0;
        is_extended = 0; 
        if (scancode == 0x12 || scancode == 0x59) is_shifted = 0;
    } else if (scancode == 0x12 || scancode == 0x59) {
        is_shifted = 1;
    } else if (scancode == 0x58) {
        is_caps = !is_caps;
    } else {
        if (scancode < 0x80) {
            int use_upper = is_shifted ^ is_caps;
            uint8_t ascii = use_upper ? scancode_set2_upper[scancode] : scancode_set2_lower[scancode];
            if (ascii != 0) {
                kbd_push_char(ascii);
            }
        }
        is_extended = 0; // Ensure extended state is cleared after processing
    }

    spin_unlock(&kbd_lock_);
}

//
// Pushing char to Circular Buffer - nessesary for Ring 3
//
void kbd_push_char(char c) {
    int next = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next != kbd_tail) {
        kbd_buffer[kbd_head] = c;
        kbd_head = next;
    }
}

//
// Pops char from Circular Buffer
//
char kbd_pop_char() {
    uint64_t f = spin_irq_save();
    spin_lock(&kbd_lock_);

    char c = 0;
    if (kbd_head != kbd_tail) {
        c = kbd_buffer[kbd_tail];
        kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    }

    spin_unlock(&kbd_lock_);
    spin_irq_restore(f);
    return c;
}
