#ifndef PS2_KBD_H
#define PS2_KBD_H

#include <stdint.h>

// Special keys (not included in ASCII)
#define KBD_KEY_UP        0x80
#define KBD_KEY_DOWN      0x81
#define KBD_KEY_LEFT      0x82
#define KBD_KEY_RIGHT     0x83
#define KBD_KEY_CAPS      0x84
#define KBD_KEY_LSHIFT    0x85
#define KBD_KEY_RSHIFT    0x86
#define KBD_KEY_LCTRL     0x87
#define KBD_KEY_LALT      0x88

// Scancode set - low keys and numbers
static const uint8_t scancode_set2_lower[] = {
    0,   0x89, 0,   0x8A, 0x8B, 0x8C, 0x8D, 0x96, // 00-07 (F)
    0,   0x97, 0x98, 0x99, 0x9A, '\t', '`', 0,    // 08-0F
    0,   0x88, 0x85, 0,   0x87, 'q', '1', 0,      // 10-17
    0,   0,   'z', 's', 'a', 'w', '2', 0,         // 18-1F
    0,   'c', 'x', 'd', 'e', '4', '3', 0,         // 20-27
    0,   ' ', 'v', 'f', 't', 'r', '5', 0,         // 28-2F
    0,   'n', 'b', 'h', 'g', 'y', '6', 0,         // 30-37
    0,   0,   'm', 'j', 'u', '7', '8', 0,         // 38-3F
    0,   ',', 'k', 'i', 'o', '0', '9', 0,         // 40-47
    0,   '.', '/', 'l', ';', 'p', '-', 0,         // 48-4F
    0,   0,   '\'',0,   '[', '=', 0,   0,         // 50-57
    0x84,0x86, '\n', ']', 0,   '\\', 0,  0,       // 58-5F (58=Caps, 59=RShift)
    0,   0,   0,   0,   0,   0,   '\b', 0,        // 60-67 (66=Backspace)
    0,   '1', 0,   '4', '7', 0,   0,   0,         // 68-6F (Keypad)
    '0', '.', '2', '5', '6', '8', 0x1B, 0,        // 70-77 (76=ESC)
    0x9B, '+', '3', '-', '*', '9', 0,   0         // 78-7F
};

// Scancode set - upper and special keys
static const uint8_t scancode_set2_upper[] = {
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   '\t', '~', 0,
    0,   0,   0,   0,   0,   'Q', '!', 0,
    0,   0,   'Z', 'S', 'A', 'W', '@', 0,
    0,   'C', 'X', 'D', 'E', '$', '#', 0,
    0,   ' ', 'V', 'F', 'T', 'R', '%', 0,
    0,   'N', 'B', 'H', 'G', 'Y', '^', 0,
    0,   0,   'M', 'J', 'U', '&', '*', 0,
    0,   '<', 'K', 'I', 'O', ')', '(', 0,
    0,   '>', '?', 'L', ':', 'P', '_', 0,
    0,   0,   '\"',0,   '{', '+', 0,   0,
    0,   0,   '\n', '}', 0,   '|', 0,   0,
    0,   0,   0,   0,   0,   0,   '\b', 0,
    0,   '1', 0,   '4', '7', 0,   0,   0,
    '0', '.', '2', '5', '6', '8', 0x1B, 0,
    0,   '=', '3', '-', '*', '9', 0,   0
};

void ps2_keyboard_handler();
void kbd_push_char(char c);
char kbd_pop_char();

#endif
