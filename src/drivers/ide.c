#include "../include/ide.h"
#include "../include/io.h"

#define IDE_DATA    0x1F0
#define IDE_SECCNT  0x1F2
#define IDE_LBA_LO  0x1F3
#define IDE_LBA_MID 0x1F4
#define IDE_LBA_HI  0x1F5
#define IDE_DRVHEAD 0x1F6
#define IDE_CMD     0x1F7   // status when read
#define IDE_CTRL    0x3F6   // device control when written, alternate status when read

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30
#define IDE_CMD_FLUSH 0xE7

#define IDE_ST_ERR 0x01
#define IDE_ST_DRQ 0x08
#define IDE_ST_BSY 0x80

void ide_init(void) {
	outb(IDE_CTRL, 0x02); // nIEN -- we only poll, never let this raise IRQ14
}

static void ide_wait_bsy(void) {
	while (inb(IDE_CMD) & IDE_ST_BSY) { }
}

static int ide_wait_drq(void) {
	for (;;) {
		unsigned char status = inb(IDE_CMD);
		if (status & IDE_ST_BSY) continue;
		if (status & IDE_ST_ERR) return -1;
		if (status & IDE_ST_DRQ) return 0;
	}
}

static void ide_select(unsigned int lba) {
	outb(IDE_SECCNT, 1);
	outb(IDE_LBA_LO, (unsigned char)(lba & 0xFF));
	outb(IDE_LBA_MID, (unsigned char)((lba >> 8) & 0xFF));
	outb(IDE_LBA_HI, (unsigned char)((lba >> 16) & 0xFF));
	outb(IDE_DRVHEAD, (unsigned char)(0xE0 | ((lba >> 24) & 0x0F)));
	// 400ns drive-select settle, read from the alternate-status port so it
	// doesn't disturb the main status register.
	inb(IDE_CTRL);
	inb(IDE_CTRL);
	inb(IDE_CTRL);
	inb(IDE_CTRL);
}

int ide_read_sectors(unsigned int lba, int count, void *buf) {
	unsigned short *p = (unsigned short *)buf;
	for (int s = 0; s < count; s++) {
		ide_wait_bsy();
		ide_select(lba + (unsigned int)s);
		outb(IDE_CMD, IDE_CMD_READ);
		if (ide_wait_drq()) {
			return -1;
		}
		for (int i = 0; i < 256; i++) {
			*p++ = inw(IDE_DATA);
		}
	}
	return 0;
}

int ide_write_sectors(unsigned int lba, int count, const void *buf) {
	const unsigned short *p = (const unsigned short *)buf;
	for (int s = 0; s < count; s++) {
		ide_wait_bsy();
		ide_select(lba + (unsigned int)s);
		outb(IDE_CMD, IDE_CMD_WRITE);
		if (ide_wait_drq()) {
			return -1;
		}
		for (int i = 0; i < 256; i++) {
			outw(IDE_DATA, *p++);
			io_wait();
		}
		ide_wait_bsy();
	}
	// One cache flush for the whole run -- without it QEMU may only keep
	// the write in its writeback cache, and it won't actually reach the
	// host .img file (the property "persists across reboot" depends on).
	ide_wait_bsy();
	outb(IDE_CMD, IDE_CMD_FLUSH);
	ide_wait_bsy();
	return 0;
}
