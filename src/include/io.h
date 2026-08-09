#ifndef _IO_H
#define _IO_H

static inline unsigned char inb(unsigned short port) {
	unsigned char result;
	__asm__ volatile ("inb %%dx, %%al" : "=a"(result) : "d"(port));
	return result;
}

static inline void outb(unsigned short port, unsigned char data) {
	__asm__ volatile ("outb %%al, %%dx" : : "a"(data), "d"(port));
}

static inline unsigned short inw(unsigned short port) {
	unsigned short result;
	__asm__ volatile ("inw %%dx, %%ax" : "=a"(result) : "d"(port));
	return result;
}

static inline void outw(unsigned short port, unsigned short data) {
	__asm__ volatile ("outw %%ax, %%dx" : : "a"(data), "d"(port));
}

// Small delay for old hardware that needs a moment between I/O port accesses.
static inline void io_wait(void) {
	__asm__ volatile ("outb %%al, $0x80" : : "a"((unsigned char)0));
}

#endif
