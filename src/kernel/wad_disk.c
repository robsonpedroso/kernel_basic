#include "../include/wad_disk.h"
#include "../include/storage_thread.h"
#include "../include/string.h"

// Staging buffer for the ragged head/tail sectors of an arbitrary-offset
// read -- same pattern as fs.c's own g_sector, kept private to this file
// since the two never need to interleave.
static unsigned char g_wad_sector[512];

int wad_read_range(unsigned int offset, void *buf, unsigned int len) {
	if (len == 0) {
		return 0;
	}
	unsigned int end = offset + len;
	if (end < offset || end > WAD_MAX_SECTORS * 512u) {
		return -1; // overflow, or past the reserved WAD region
	}

	unsigned char *dst = (unsigned char *)buf;

	unsigned int head_off = offset % 512u;
	if (head_off != 0) {
		storage_read_sectors(WAD_LBA + offset / 512u, 1, g_wad_sector);
		unsigned int avail = 512u - head_off;
		unsigned int take = (len < avail) ? len : avail;
		memcpy(dst, g_wad_sector + head_off, (int)take);
		dst += take;
		offset += take;
		len -= take;
	}
	if (len == 0) {
		return 0;
	}

	unsigned int whole = len / 512u;
	if (whole > 0) {
		storage_read_sectors(WAD_LBA + offset / 512u, (int)whole, dst);
		dst += whole * 512u;
		offset += whole * 512u;
		len -= whole * 512u;
	}

	if (len > 0) {
		storage_read_sectors(WAD_LBA + offset / 512u, 1, g_wad_sector);
		memcpy(dst, g_wad_sector, (int)len);
	}

	return 0;
}
