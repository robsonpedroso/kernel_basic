#ifndef _WAD_DISK_H
#define _WAD_DISK_H

// Fixed disk region for the Doom IWAD blob, living right after fs.c's
// filesystem region (FS_END_LBA in fs.h) inside the enlarged disk image
// (see Makefile's image-size bump to 32768 sectors / 16MiB). Provisioned
// at build time via `dd` (see Makefile) -- there is no runtime write path,
// and the WAD is never registered as an fs.c fs_dirent_t entry. Kept
// deliberately separate from fs.c/FS_MAX_FILE_SIZE: raising that constant
// would grow a .bss-resident scratch buffer (fs.c's g_copy_buf) by the
// same amount, which counts directly against link.ld's image-size budget
// for no good reason -- the WAD is a read-only asset, not something the
// File Manager's copy/rename UI needs to understand.
// WAD_LBA = fs.h's FS_END_LBA (moves in lockstep with it -- see that
// header's comment on why: both shifted together when IDE_SECTORS grew).
#define WAD_LBA         6176u
#define WAD_MAX_SECTORS 10240u // 5MiB -- doom1.wad (shareware) is ~4.2MB

// Reads `len` bytes starting at byte `offset` within the WAD region into
// buf. Unlike fs_read_file (whole-file-from-0 only), this supports an
// arbitrary starting offset, matching doomgeneric's w_wad.c
// fseek/fread-style access pattern. Returns 0 on success, -1 if the range
// falls outside WAD_MAX_SECTORS*512.
int wad_read_range(unsigned int offset, void *buf, unsigned int len);

#endif
