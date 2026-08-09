#ifndef _FS_H
#define _FS_H

// Flat-table filesystem living in the tail of the 1.44MB disk image that
// nothing else touches (the bootloader only ever reads LBA 1..800 into the
// kernel). One superblock sector, a fixed-size directory table, and a
// bump-allocated data region. Deliberately no free-space map: deleting a
// file, or growing one past its already-reserved run, leaks its old
// sectors until the region is reformatted (see fs.c). With ~1MB of data
// region and small text files, a bitmap+compactor would be more code than
// the rest of the filesystem put together -- not worth it for this scope.

// v2: entries grew from 64 to 128 bytes to fit attr + timestamps (see
// fs_dirent_t below). FS_VERSION is checked by fs_init() -- a v1 image (or
// anything else with a mismatched version) is reformatted exactly like a
// bad magic, so upgrading wipes whatever was on disk. There is no
// migration tool; this is the same tradeoff as running `make distclean`.
#define FS_VERSION       2u

#define FS_SUPER_LBA     801u
#define FS_TABLE_LBA     802u
#define FS_TABLE_SECTORS 32u     // 128 entries * 128 bytes / 512
#define FS_DATA_LBA      834u    // FS_TABLE_LBA + FS_TABLE_SECTORS
#define FS_END_LBA       2880u   // 1474560 / 512

#define FS_MAX_ENTRIES   128
#define FS_NAME_MAX      100     // 99 chars + NUL -- pads fs_dirent_t to 128 bytes
#define FS_MAX_FILE_SIZE 4096    // matches EDITBUF_CAPACITY

#define FS_TYPE_FILE     1
#define FS_TYPE_FOLDER   2

// attr bitmask -- display-only for now, and nothing currently writes it
// (there is no fs_set_attr()): nothing in fs.c enforces FS_ATTR_RO against
// fs_delete/fs_write_file yet.
#define FS_ATTR_RO      0x01
#define FS_ATTR_HIDDEN  0x02
#define FS_ATTR_SYSTEM  0x04
#define FS_ATTR_ARCHIVE 0x08

// The root directory is implicit -- it has no table entry, it is just the
// parent value every top-level entry carries.
#define FS_ROOT (-1)

// Distinct from FS_ROOT on purpose: fs_resolve_path() can legitimately
// resolve to FS_ROOT (e.g. ".." from a top-level folder), so it needs a
// failure value other than -1 -- unlike fs_find()/fs_create_*/etc., which
// never produce FS_ROOT as a successful result and so can safely use -1
// for "not found".
#define FS_NOT_FOUND (-2)

typedef struct fs_dirent {
	unsigned char used;       // 0 = free slot
	unsigned char type;       // FS_TYPE_FILE | FS_TYPE_FOLDER
	unsigned char attr;       // FS_ATTR_* bitmask
	unsigned char reserved0;
	short         parent;     // entry id, or FS_ROOT
	short         reserved1;
	unsigned int  start_lba;  // files only
	unsigned int  size;       // files only, bytes
	unsigned int  alloc;      // files only, sectors reserved at start_lba
	unsigned int  created;    // packed FAT-style date/time, see rtc_now()
	unsigned int  modified;   // 0 if the RTC driver isn't wired in
	char          name[FS_NAME_MAX];
} fs_dirent_t; // exactly 128 bytes

// Classic negative-array-size compile-time check -- this codebase predates
// C11 and doesn't use _Static_assert anywhere else, so stay consistent.
typedef char fs_dirent_size_check[(sizeof(fs_dirent_t) == 128) ? 1 : -1];

void fs_init(void);

int  fs_list(int parent, int *out_ids, int max);    // returns count written
const fs_dirent_t *fs_get(int id);                   // 0 for an invalid/free id
int  fs_find(int parent, const char *name);          // entry id, or -1

int  fs_create_file(int parent, const char *name);   // new id, or -1
int  fs_create_folder(int parent, const char *name);
int  fs_delete(int id);                               // 0, or -1 if folder non-empty

int  fs_read_file(int id, char *buf, int max);        // bytes read, or -1
int  fs_write_file(int id, const char *buf, int len);  // 0, or -1

int  fs_rename(int id, const char *new_name);          // 0, or -1 (bad name/dup)
int  fs_move(int id, int new_parent);                   // 0, or -1 (cycle/bad parent)
int  fs_copy(int id, int new_parent, const char *name_or_null); // new id, or -1
int  fs_path(int id, char *buf, int max);                // materializes "\A\B\C"
int  fs_free_sectors(void);                                  // data sectors left

// Resolves a "\A\B\C"-style path string to an entry id, or FS_NOT_FOUND if
// any segment along the way doesn't exist. A leading '\' anchors at
// FS_ROOT; otherwise resolution starts at `cwd`. "." is a no-op segment,
// ".." moves to the parent (staying at FS_ROOT if already there). May
// return FS_ROOT itself (e.g. resolving "\" or ".." from a top-level
// folder) -- that is a valid result, not an error, which is why failure is
// FS_NOT_FOUND rather than -1 (== FS_ROOT).
int  fs_resolve_path(int cwd, const char *path);

#endif
