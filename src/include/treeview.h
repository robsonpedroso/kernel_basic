#ifndef _TREEVIEW_H
#define _TREEVIEW_H

#include "rect.h"

// Unlike listbox.h, this widget is inherently tied to the fs.h tree model
// (real winfile's tree pane only ever shows folders too), so treeview.c
// includes fs.h directly instead of abstracting it away. Row cap mirrors
// fs.h's FS_MAX_ENTRIES exactly (every entry could theoretically be a
// folder); treeview.c checks the two can't silently drift apart.
#define TREEVIEW_MAX_ROWS 128

typedef struct treeview_row {
	int fs_id;         // FS_ROOT for the synthetic "C:\" root row
	int depth;
	int has_children;  // any FS_TYPE_FOLDER child exists
} treeview_row_t;

typedef struct treeview {
	rect_st rect;                              // content-relative
	treeview_row_t rows[TREEVIEW_MAX_ROWS];
	int row_count;
	int top;                                    // scroll offset, in rows
	unsigned char expanded[TREEVIEW_MAX_ROWS];   // bitset by fs id (root excluded, see below)
	int selected_id;                             // == caller's current cwd, or FS_ROOT
} treeview_st;

void treeview_init(treeview_st *tv, rect_st rect);

// Walks fs_list() from FS_ROOT, descending only into folders marked
// expanded[]. The root row itself is always shown expanded (this model
// doesn't support collapsing it). Call after any fs mutation that could
// change the folder set (create/delete/move/rename) or after toggling a
// row's expand state.
void treeview_rebuild(treeview_st *tv);

void treeview_draw(treeview_st *tv);

// -1 if (x,y) isn't over a row, else an index into tv->rows[].
int treeview_hit_row(treeview_st *tv, int x, int y);

// 1 if (x,y) is over that row's +/- toggle box (row already resolved via
// treeview_hit_row).
int treeview_hit_toggle(treeview_st *tv, int row, int x, int y);

// Flips that row's expand state and rebuilds. No-op for the root row.
void treeview_toggle_expand(treeview_st *tv, int row);

#endif
