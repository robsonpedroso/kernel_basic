#include "../include/rtc.h"
#include "../include/io.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static unsigned char cmos_read(unsigned char reg) {
	outb(CMOS_ADDR, reg);
	return inb(CMOS_DATA);
}

static int cmos_update_in_progress(void) {
	outb(CMOS_ADDR, 0x0A);
	return inb(CMOS_DATA) & 0x80;
}

static unsigned char bcd_to_bin(unsigned char v) {
	return (unsigned char)((v & 0x0F) + ((v >> 4) * 10));
}

unsigned int rtc_now(void) {
	while (cmos_update_in_progress()) { }

	unsigned char sec  = cmos_read(0x00);
	unsigned char min  = cmos_read(0x02);
	unsigned char hour = cmos_read(0x04);
	unsigned char day  = cmos_read(0x07);
	unsigned char mon  = cmos_read(0x08);
	unsigned char year = cmos_read(0x09); // 2-digit, e.g. 25 for 2025
	unsigned char regb = cmos_read(0x0B);

	if (!(regb & 0x04)) { // bit clear = BCD, set = already binary
		sec  = bcd_to_bin(sec);
		min  = bcd_to_bin(min);
		hour = (unsigned char)(bcd_to_bin(hour & 0x7F) | (hour & 0x80));
		day  = bcd_to_bin(day);
		mon  = bcd_to_bin(mon);
		year = bcd_to_bin(year);
	}
	if (!(regb & 0x02) && (hour & 0x80)) { // 12-hour mode, PM bit set
		hour = (unsigned char)(((hour & 0x7F) + 12) % 24);
	}

	// No CMOS century register read here -- assume the 2000s, fine for a
	// hobby OS with no century rollover to worry about for a long while.
	unsigned int fat_year = 2000u + year - 1980u;
	if (fat_year > 127u) {
		fat_year = 127u;
	}

	unsigned int packed = 0;
	packed |= (fat_year & 0x7Fu) << 25;
	packed |= ((unsigned int)mon  & 0x0Fu) << 21;
	packed |= ((unsigned int)day  & 0x1Fu) << 16;
	packed |= ((unsigned int)hour & 0x1Fu) << 11;
	packed |= ((unsigned int)min  & 0x3Fu) << 5;
	packed |= ((unsigned int)(sec / 2) & 0x1Fu);
	return packed;
}
