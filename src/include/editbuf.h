#ifndef _EDITBUF_H
#define _EDITBUF_H

#include "textbox.h"

#define EDITBUF_CAPACITY 4096 // matches FS_MAX_FILE_SIZE

// A flat, cursor-navigable text buffer. Unlike textbox_st (an append-only
// console grid whose scroll permanently discards row 0), this owns the
// whole document -- arbitrary insert/delete, a byte-offset cursor, and a
// scrollable viewport -- and only uses a textbox_st as a dumb render
// target for whatever slice is currently in view.
typedef struct editbuf {
	char *data;      // kmalloc'd, cap bytes, NOT NUL-terminated
	int   len;
	int   cap;
	int   cursor;    // byte offset, 0..len
	int   goal_col;  // sticky column for up/down; -1 = recompute from cursor
	int   top_line;  // first document line shown in the viewport
	int   dirty;     // modified since the last load/save
} editbuf_st;

void editbuf_init(editbuf_st *eb, int capacity);
void editbuf_free(editbuf_st *eb);
void editbuf_load(editbuf_st *eb, const char *src, int len); // replaces all, cursor=0

// Handles one key from on_key_down: printable, '\n', '\b', and the KEY_*
// pseudo-ASCII codes from keyboard.h. Returns 1 if the buffer contents
// changed (cursor-only moves return 0), so the caller can track `dirty`.
int editbuf_key(editbuf_st *eb, int ascii);

// Repopulates tb's cell grid from the viewport slice, scrolling top_line so
// the cursor stays visible, and sets tb->cursor_row/col for textbox_draw.
void editbuf_render(editbuf_st *eb, textbox_st *tb);

#endif
