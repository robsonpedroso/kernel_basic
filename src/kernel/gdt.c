#include "../include/gdt.h"

struct gdt_entry {
	unsigned short limit_low;
	unsigned short base_low;
	unsigned char  base_mid;
	unsigned char  access;
	unsigned char  granularity;
	unsigned char  base_high;
} __attribute__((packed));

struct gdt_ptr {
	unsigned short limit;
	unsigned int   base;
} __attribute__((packed));

// 3 entries, matching bootloader.asm's original table exactly: null, code
// (selector 0x08), data (selector 0x10) -- same selectors, same flat 4GB
// layout, just living in permanent kernel memory instead of the boot
// sector.
static struct gdt_entry gdt[3];
static struct gdt_ptr   gdtp;

extern void gdt_flush(unsigned int);

static void gdt_set_gate(int num, unsigned int base, unsigned int limit, unsigned char access, unsigned char gran) {
	gdt[num].base_low = base & 0xFFFF;
	gdt[num].base_mid = (base >> 16) & 0xFF;
	gdt[num].base_high = (base >> 24) & 0xFF;

	gdt[num].limit_low = limit & 0xFFFF;
	gdt[num].granularity = (limit >> 16) & 0x0F;
	gdt[num].granularity |= gran & 0xF0;

	gdt[num].access = access;
}

void gdt_install(void) {
	gdtp.limit = sizeof(struct gdt_entry) * 3 - 1;
	gdtp.base = (unsigned int)&gdt[0];

	gdt_set_gate(0, 0, 0, 0, 0);                     // null descriptor
	gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);       // code: present, ring0, exec/read, 4KB gran, 32-bit
	gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);       // data: present, ring0, read/write, 4KB gran, 32-bit

	gdt_flush((unsigned int)&gdtp);
}
