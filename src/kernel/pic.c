#include "../include/pic.h"
#include "../include/io.h"

void pic_remap(void) {
	unsigned char mask1 = inb(PIC1_DATA);
	unsigned char mask2 = inb(PIC2_DATA);

	outb(PIC1_COMMAND, 0x11); // ICW1: init, expect ICW4
	io_wait();
	outb(PIC2_COMMAND, 0x11);
	io_wait();

	outb(PIC1_DATA, PIC1_OFFSET); // ICW2: vector offset
	io_wait();
	outb(PIC2_DATA, PIC2_OFFSET);
	io_wait();

	outb(PIC1_DATA, 0x04); // ICW3: master has slave at IRQ2
	io_wait();
	outb(PIC2_DATA, 0x02); // ICW3: slave cascade identity
	io_wait();

	outb(PIC1_DATA, 0x01); // ICW4: 8086 mode
	io_wait();
	outb(PIC2_DATA, 0x01);
	io_wait();

	outb(PIC1_DATA, mask1); // restore saved masks
	outb(PIC2_DATA, mask2);
}

void pic_send_eoi(unsigned char irq) {
	if (irq >= 8) {
		outb(PIC2_COMMAND, 0x20);
	}
	outb(PIC1_COMMAND, 0x20);
}

void pic_clear_mask(unsigned char irq) {
	unsigned short port = irq < 8 ? PIC1_DATA : PIC2_DATA;
	unsigned char bit = irq < 8 ? irq : irq - 8;
	unsigned char value = inb(port) & ~(1 << bit);
	outb(port, value);
}
