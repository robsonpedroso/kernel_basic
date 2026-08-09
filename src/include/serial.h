#ifndef _SERIAL_H
#define _SERIAL_H

// Minimal COM1 serial output, used only as a debug/bring-up aid: lets us
// trace boot progress (IDT/PIC/timer/keyboard/mouse init, IRQ firing) from
// outside the emulator without a screen, since QEmu can redirect COM1 to a
// log file (-serial file:...).
void serial_init(void);
void serial_write_char(char c);
void serial_write(const char *s);
void serial_write_hex(unsigned int value);

#endif
