#ifndef _IDE_H
#define _IDE_H

// Primary ATA channel, master drive, LBA28, polled PIO -- the same protocol
// bootloader.asm's ide_load_kernel uses to load the kernel, but reachable
// from C after boot and with WRITE added (the boot-time loader is
// read-only and unreachable once the kernel is running).
void ide_init(void);

// buf must hold count*512 bytes. Returns 0 on success, -1 if the drive
// raised ERR.
int ide_read_sectors(unsigned int lba, int count, void *buf);
int ide_write_sectors(unsigned int lba, int count, const void *buf);

#endif
