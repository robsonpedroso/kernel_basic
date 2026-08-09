#include "../include/idt.h"

#define IDT_ENTRIES 256

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

extern void idt_flush(unsigned int);

void idt_set_gate(unsigned char num, unsigned int base, unsigned short sel, unsigned char flags) {
	idt[num].base_low = base & 0xFFFF;
	idt[num].base_high = (base >> 16) & 0xFFFF;
	idt[num].sel = sel;
	idt[num].always0 = 0;
	idt[num].flags = flags;
}

void idt_install(void) {
	idtp.limit = sizeof(struct idt_entry) * IDT_ENTRIES - 1;
	idtp.base = (unsigned int)&idt[0];

	for (int i = 0; i < IDT_ENTRIES; i++) {
		idt_set_gate(i, 0, 0, 0);
	}

	idt_flush((unsigned int)&idtp);
}
