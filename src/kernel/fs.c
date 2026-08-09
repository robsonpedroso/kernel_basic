#include "../include/fs.h"
#include "../include/ide.h"
#include "../include/rtc.h"
#include "../include/string.h"

#define FS_MAGIC 0x72534653u // 'rSFS'

typedef struct fs_super {
	unsigned int magic;
	unsigned int version;
	unsigned int next_free; // bump cursor into the data region
	unsigned int reserved;
} fs_super_t;

static fs_super_t    g_super;
static fs_dirent_t   g_table[FS_MAX_ENTRIES]; // 16KB, .bss (not the 128KB heap)
static unsigned char g_sector[512];           // staging buffer for ragged tails
static char          g_copy_buf[FS_MAX_FILE_SIZE]; // fs_copy's scratch, not the stack

static int fs_name_eq(const char *a, const char *b) {
	while (*a && *b) {
		if (*a != *b) {
			return 0;
		}
		a++;
		b++;
	}
	return *a == *b; // both hit NUL together, or neither did
}

static int fs_name_len(const char *s) {
	int n = 0;
	while (s[n]) {
		n++;
	}
	return n;
}

// string.c's strcpy never writes the terminator -- write our own that does.
static void fs_set_name(fs_dirent_t *e, const char *name) {
	int i = 0;
	while (name[i] && i < FS_NAME_MAX - 1) {
		e->name[i] = name[i];
		i++;
	}
	e->name[i] = 0;
}

static void fs_flush_super(void) {
	memset(g_sector, 0, 512);
	*(fs_super_t *)g_sector = g_super;
	ide_write_sectors(FS_SUPER_LBA, 1, g_sector);
}

static void fs_flush_entry(int id) {
	// 4 entries (128 bytes each) per 512-byte sector.
	int sector = id / 4;
	ide_write_sectors(FS_TABLE_LBA + (unsigned int)sector, 1,
	                   (unsigned char *)g_table + (unsigned int)sector * 512u);
}

static void fs_format(void) {
	for (int i = 0; i < FS_MAX_ENTRIES; i++) {
		g_table[i].used = 0;
	}
	ide_write_sectors(FS_TABLE_LBA, (int)FS_TABLE_SECTORS, g_table);

	g_super.magic = FS_MAGIC;
	g_super.version = FS_VERSION;
	g_super.next_free = FS_DATA_LBA;
	g_super.reserved = 0;
	fs_flush_super();
}

void fs_init(void) {
	ide_read_sectors(FS_SUPER_LBA, 1, g_sector);
	g_super = *(fs_super_t *)g_sector;

	// A version mismatch (e.g. an old v1 image) is treated exactly like a
	// bad magic: the v1->v2 entry layout changed size (64 -> 128 bytes),
	// so reading old table bytes under the new stride would silently
	// misinterpret them instead of failing loudly. There is no migration
	// tool -- this wipes whatever was on disk, same as `make distclean`.
	if (g_super.magic != FS_MAGIC || g_super.version != FS_VERSION ||
	    g_super.next_free < FS_DATA_LBA || g_super.next_free > FS_END_LBA) {
		fs_format(); // blank/corrupt/stale disk (or first-ever boot) -- auto-format
		return;
	}
	ide_read_sectors(FS_TABLE_LBA, (int)FS_TABLE_SECTORS, g_table);
}

static int fs_alloc_entry(void) {
	for (int i = 0; i < FS_MAX_ENTRIES; i++) {
		if (!g_table[i].used) {
			return i;
		}
	}
	return -1;
}

static int fs_parent_ok(int parent) {
	if (parent == FS_ROOT) {
		return 1;
	}
	if (parent < 0 || parent >= FS_MAX_ENTRIES) {
		return 0;
	}
	return g_table[parent].used && g_table[parent].type == FS_TYPE_FOLDER;
}

static int fs_valid(int id) {
	return id >= 0 && id < FS_MAX_ENTRIES && g_table[id].used;
}

const fs_dirent_t *fs_get(int id) {
	if (!fs_valid(id)) {
		return 0;
	}
	return &g_table[id];
}

int fs_find(int parent, const char *name) {
	for (int i = 0; i < FS_MAX_ENTRIES; i++) {
		if (g_table[i].used && g_table[i].parent == parent && fs_name_eq(g_table[i].name, name)) {
			return i;
		}
	}
	return -1;
}

int fs_list(int parent, int *out_ids, int max) {
	int count = 0;
	for (int i = 0; i < FS_MAX_ENTRIES && count < max; i++) {
		if (g_table[i].used && g_table[i].parent == parent) {
			out_ids[count++] = i;
		}
	}
	return count;
}

static int fs_create(int parent, const char *name, int type) {
	if (!fs_parent_ok(parent)) {
		return -1;
	}
	int len = fs_name_len(name);
	if (len == 0 || len > FS_NAME_MAX - 1) {
		return -1;
	}
	if (fs_find(parent, name) >= 0) {
		return -1;
	}
	int id = fs_alloc_entry();
	if (id < 0) {
		return -1;
	}
	fs_dirent_t *e = &g_table[id];
	e->used = 1;
	e->type = (unsigned char)type;
	e->attr = 0;
	e->reserved0 = 0;
	e->parent = (short)parent;
	e->reserved1 = 0;
	e->start_lba = 0;
	e->size = 0;
	e->alloc = 0;
	e->created = rtc_now();
	e->modified = e->created;
	fs_set_name(e, name);
	fs_flush_entry(id);
	return id;
}

int fs_create_file(int parent, const char *name) {
	return fs_create(parent, name, FS_TYPE_FILE);
}

int fs_create_folder(int parent, const char *name) {
	return fs_create(parent, name, FS_TYPE_FOLDER);
}

int fs_delete(int id) {
	if (!fs_valid(id)) {
		return -1;
	}
	if (g_table[id].type == FS_TYPE_FOLDER) {
		for (int i = 0; i < FS_MAX_ENTRIES; i++) {
			if (g_table[i].used && g_table[i].parent == id) {
				return -1; // folder not empty
			}
		}
	}
	g_table[id].used = 0;
	fs_flush_entry(id);
	return 0;
}

int fs_write_file(int id, const char *buf, int len) {
	if (!fs_valid(id) || g_table[id].type != FS_TYPE_FILE) {
		return -1;
	}
	if (len < 0 || len > FS_MAX_FILE_SIZE) {
		return -1;
	}
	fs_dirent_t *e = &g_table[id];
	unsigned int need = ((unsigned int)len + 511u) / 512u;

	// Growing past the already-reserved run allocates a fresh one (bump
	// allocator, see fs.h) -- the old run leaks. Staying within it (same
	// size or shrinking) is free.
	if (need > e->alloc) {
		if (g_super.next_free + need > FS_END_LBA) {
			return -1; // disk full
		}
		e->start_lba = g_super.next_free;
		e->alloc = need;
		g_super.next_free += need;
		fs_flush_super();
	}

	unsigned int whole = (unsigned int)len / 512u;
	if (whole > 0) {
		ide_write_sectors(e->start_lba, (int)whole, buf);
	}
	unsigned int rest = (unsigned int)len - whole * 512u;
	if (rest > 0) {
		memset(g_sector, 0, 512);
		memcpy(g_sector, buf + whole * 512u, (int)rest);
		ide_write_sectors(e->start_lba + whole, 1, g_sector);
	}

	e->size = (unsigned int)len;
	e->modified = rtc_now();
	fs_flush_entry(id);
	return 0;
}

int fs_read_file(int id, char *buf, int max) {
	if (!fs_valid(id) || g_table[id].type != FS_TYPE_FILE) {
		return -1;
	}
	fs_dirent_t *e = &g_table[id];
	int n = (int)e->size;
	if (n > max) {
		n = max;
	}
	if (n <= 0) {
		return 0;
	}

	unsigned int whole = (unsigned int)n / 512u;
	if (whole > 0) {
		ide_read_sectors(e->start_lba, (int)whole, buf);
	}
	unsigned int rest = (unsigned int)n - whole * 512u;
	if (rest > 0) {
		ide_read_sectors(e->start_lba + whole, 1, g_sector);
		memcpy(buf + whole * 512u, g_sector, (int)rest);
	}
	return n;
}

int fs_rename(int id, const char *new_name) {
	if (!fs_valid(id)) {
		return -1;
	}
	int len = fs_name_len(new_name);
	if (len == 0 || len > FS_NAME_MAX - 1) {
		return -1;
	}
	fs_dirent_t *e = &g_table[id];
	int existing = fs_find(e->parent, new_name);
	if (existing >= 0 && existing != id) {
		return -1; // a sibling already has that name
	}
	fs_set_name(e, new_name);
	fs_flush_entry(id);
	return 0;
}

int fs_move(int id, int new_parent) {
	if (!fs_valid(id)) {
		return -1;
	}
	if (!fs_parent_ok(new_parent) || new_parent == id) {
		return -1;
	}
	// Reject moving a folder into its own descendant: walk up from
	// new_parent toward FS_ROOT: hitting id anywhere on the way up means
	// new_parent lives inside id. Bounded by FS_MAX_ENTRIES so a corrupt
	// parent chain can't loop forever.
	int cur = new_parent;
	for (int steps = 0; cur != FS_ROOT && steps < FS_MAX_ENTRIES; steps++) {
		if (cur == id) {
			return -1;
		}
		cur = g_table[cur].parent;
	}
	if (fs_find(new_parent, g_table[id].name) >= 0) {
		return -1; // name collision in destination
	}
	g_table[id].parent = (short)new_parent;
	fs_flush_entry(id);
	return 0;
}

static int fs_copy_depth(int id, int new_parent, const char *name_or_null, int depth) {
	if (!fs_valid(id)) {
		return -1;
	}
	if (!fs_parent_ok(new_parent)) {
		return -1;
	}
	if (depth > 16) {
		return -1; // defensive cap against a pathological tree, not expected in practice
	}
	const fs_dirent_t *src = &g_table[id];
	if (src->type == FS_TYPE_FOLDER && new_parent == id) {
		return -1; // copying a folder into itself would self-nest forever
	}
	const char *name = name_or_null ? name_or_null : src->name;

	if (src->type == FS_TYPE_FILE) {
		int new_id = fs_create_file(new_parent, name);
		if (new_id < 0) {
			return -1;
		}
		int n = fs_read_file(id, g_copy_buf, FS_MAX_FILE_SIZE);
		if (n > 0 && fs_write_file(new_id, g_copy_buf, n) != 0) {
			fs_delete(new_id);
			return -1;
		}
		return new_id;
	}

	int new_id = fs_create_folder(new_parent, name);
	if (new_id < 0) {
		return -1;
	}
	int ids[FS_MAX_ENTRIES];
	int count = fs_list(id, ids, FS_MAX_ENTRIES);
	for (int i = 0; i < count; i++) {
		if (fs_copy_depth(ids[i], new_id, 0, depth + 1) < 0) {
			return -1;
		}
	}
	return new_id;
}

int fs_copy(int id, int new_parent, const char *name_or_null) {
	return fs_copy_depth(id, new_parent, name_or_null, 0);
}

int fs_path(int id, char *buf, int max) {
	if (max <= 0) {
		return -1;
	}
	if (id != FS_ROOT && !fs_valid(id)) {
		return -1;
	}

	// Walk the parent chain into a stack of ids (bounded by FS_MAX_ENTRIES,
	// a corrupt cycle can't loop forever), then render root-to-leaf.
	int chain[FS_MAX_ENTRIES];
	int depth = 0;
	int cur = id;
	while (cur != FS_ROOT && depth < FS_MAX_ENTRIES) {
		chain[depth++] = cur;
		cur = g_table[cur].parent;
	}

	int pos = 0;
	for (int i = depth - 1; i >= 0 && pos < max - 1; i--) {
		buf[pos++] = '\\';
		const char *name = g_table[chain[i]].name;
		int j = 0;
		while (name[j] && pos < max - 1) {
			buf[pos++] = name[j++];
		}
	}
	if (pos == 0) {
		if (max < 2) {
			return -1;
		}
		buf[pos++] = '\\';
	}
	buf[pos] = 0;
	return 0;
}

int fs_free_sectors(void) {
	if (g_super.next_free > FS_END_LBA) {
		return 0;
	}
	return (int)(FS_END_LBA - g_super.next_free);
}

int fs_resolve_path(int cwd, const char *path) {
	if (!path) {
		return FS_NOT_FOUND;
	}
	int cur = (path[0] == '\\') ? FS_ROOT : cwd;
	int i = 0;
	while (path[i]) {
		while (path[i] == '\\') {
			i++;
		}
		if (!path[i]) {
			break;
		}
		char seg[FS_NAME_MAX];
		int len = 0;
		while (path[i] && path[i] != '\\' && len < FS_NAME_MAX - 1) {
			seg[len++] = path[i++];
		}
		seg[len] = 0;

		if (len == 1 && seg[0] == '.') {
			continue;
		}
		if (len == 2 && seg[0] == '.' && seg[1] == '.') {
			if (cur != FS_ROOT) {
				const fs_dirent_t *e = fs_get(cur);
				cur = e ? e->parent : FS_ROOT;
			}
			continue;
		}
		int next = fs_find(cur, seg);
		if (next < 0) {
			return FS_NOT_FOUND;
		}
		cur = next;
	}
	return cur;
}
