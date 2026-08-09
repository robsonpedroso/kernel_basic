#include "../include/keyboard.h"
#include "../include/isr.h"
#include "../include/event.h"
#include "../include/io.h"

// US QWERTY scancode set 1 -> ASCII. Index is the scancode with the
// make/break bit (0x80) stripped off.
static const unsigned char keyboard_ascii[128] =
{
	0,  27, '1', '2', '3', '4', '5', '6', '7', '8',
	'9', '0', '-', '=', '\b',	/* Backspace */
	'\t',
	'q', 'w', 'e', 'r',	/* 19 */
	't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
	0,			/* 29   - Control (handled separately below) */
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
	'\'', '`',   0,		/* Left shift (handled separately below) */
	'\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
	'm', ',', '.', '/',   0,				/* Right shift */
	'*',
	0,	/* Alt */
	' ',	/* Space bar */
	0,	/* Caps lock */
	0,	/* 59 - F1 key ... > */
	0,   0,   0,   0,   0,   0,   0,   0,
	0,	/* < ... F10 */
	0,	/* 69 - Num lock*/
	0,	/* Scroll Lock */
	0,	/* Home key */
	0,	/* Up Arrow */
	0,	/* Page Up */
	'-',
	0,	/* Left Arrow */
	0,
	0,	/* Right Arrow */
	'+',
	0,	/* 79 - End key*/
	0,	/* Down Arrow */
	0,	/* Page Down */
	0,	/* Insert Key */
	0,	/* Delete Key */
	0,   0,   0,
	0,	/* F11 Key */
	0,	/* F12 Key */
	0,	/* All other keys are undefined */
};

// Same layout with Shift applied (letters upper-case, shifted symbols).
static const unsigned char keyboard_ascii_shift[128] =
{
	0,  27, '!', '@', '#', '$', '%', '^', '&', '*',
	'(', ')', '_', '+', '\b',
	'\t',
	'Q', 'W', 'E', 'R',
	'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
	0,
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
	'"', '~',   0,
	'|', 'Z', 'X', 'C', 'V', 'B', 'N',
	'M', '<', '>', '?',   0,
	'*',
	0,
	' ',
	0,
	0,
	0,   0,   0,   0,   0,   0,   0,   0,
	0,
	0,
	0,
	0,
	0,
	0,
	'-',
	0,
	0,
	0,
	'+',
	0,
	0,
	0,
	0,
	0,
	0,   0,   0,
	0,
	0,
	0,
};

#define SC_LSHIFT_MAKE  0x2A
#define SC_RSHIFT_MAKE  0x36
#define SC_LSHIFT_BREAK 0xAA
#define SC_RSHIFT_BREAK 0xB6
#define SC_CTRL_MAKE    0x1D
#define SC_CTRL_BREAK   0x9D
#define SC_ALT_MAKE     0x38
#define SC_ALT_BREAK    0xB8
#define SC_EXTENDED     0xE0

static int mods = 0;
static int extended = 0; // last byte was the 0xE0 prefix -- next byte is extended

// The 0xE0-prefixed scancodes an app actually reacts to. Anything else
// (media keys, the numpad-cluster duplicates, etc.) returns 0 and is
// dropped, same as an unmapped normal scancode.
static int keyboard_extended_ascii(unsigned char index) {
	switch (index) {
		case 0x4B: return KEY_LEFT;
		case 0x4D: return KEY_RIGHT;
		case 0x48: return KEY_UP;
		case 0x50: return KEY_DOWN;
		case 0x53: return KEY_DELETE;
		case 0x47: return KEY_HOME;
		case 0x4F: return KEY_END;
	}
	return 0;
}

static void keyboard_irq_handler(registers_t *regs) {
	(void)regs;

	unsigned char scancode = inb(0x60);

	if (scancode == SC_EXTENDED) {
		extended = 1;
		return;
	}

	if (extended) {
		extended = 0;
		// Right Ctrl and AltGr arrive extended too -- keep the modifier
		// mask honest instead of silently dropping them.
		switch (scancode) {
			case 0x1D: mods |= KEY_MOD_CTRL;  return;
			case 0x9D: mods &= ~KEY_MOD_CTRL; return;
			case 0x38: mods |= KEY_MOD_ALT;   return;
			case 0xB8: mods &= ~KEY_MOD_ALT;  return;
		}
		int is_break = scancode & 0x80;
		int ascii = keyboard_extended_ascii(scancode & 0x7F);
		if (ascii == 0) {
			return;
		}
		event_push(is_break ? EVENT_KEY_UP : EVENT_KEY_DOWN, ascii, mods);
		return;
	}

	switch (scancode) {
		case SC_LSHIFT_MAKE:
		case SC_RSHIFT_MAKE:
			mods |= KEY_MOD_SHIFT;
			return;
		case SC_LSHIFT_BREAK:
		case SC_RSHIFT_BREAK:
			mods &= ~KEY_MOD_SHIFT;
			return;
		case SC_CTRL_MAKE:
			mods |= KEY_MOD_CTRL;
			return;
		case SC_CTRL_BREAK:
			mods &= ~KEY_MOD_CTRL;
			return;
		case SC_ALT_MAKE:
			mods |= KEY_MOD_ALT;
			return;
		case SC_ALT_BREAK:
			mods &= ~KEY_MOD_ALT;
			return;
	}

	int is_break = scancode & 0x80;
	unsigned char index = scancode & 0x7F;
	if (index >= 128) {
		return;
	}

	unsigned char ascii = (mods & KEY_MOD_SHIFT) ? keyboard_ascii_shift[index] : keyboard_ascii[index];
	if (ascii == 0) {
		return;
	}

	event_push(is_break ? EVENT_KEY_UP : EVENT_KEY_DOWN, ascii, mods);
}

void keyboard_init(void) {
	mods = 0;
	extended = 0;
	irq_install_handler(1, keyboard_irq_handler);
}

unsigned int keyboard_get_mods(void) {
	return (unsigned int)mods;
}
