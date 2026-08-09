#ifndef _LISTBOX_H
#define _LISTBOX_H

#include "rect.h"

// Upper bound on rows in a single listbox -- matches fs.h's FS_MAX_ENTRIES
// (the largest a File Manager directory listing can ever be), duplicated
// here as a literal so this generic widget doesn't depend on fs.h.
#define LISTBOX_MAX_ROWS 128

// Replaces file_manager.c v1's textbox_st-abuse for the file listing: a
// thin view over a caller-owned row array, with per-row selection state
// (single click, Ctrl-toggle, Shift-range) and vertical scrolling.
typedef struct listbox {
	rect_st rect;                              // content-relative
	int count;                                 // rows currently shown
	int top;                                    // scroll offset, first visible row
	unsigned char selected[LISTBOX_MAX_ROWS];    // 1 = selected, indexed by row
	int anchor_row;                              // Shift-click range start, -1 = none
} listbox_st;

void listbox_init(listbox_st *lb, rect_st rect);

// labels[i] is row i's already-formatted text (e.g. File Manager prefixes
// folders with "[D] "). Draws only the rows that fit in lb->rect, honoring
// lb->top; also records `count` for hit-testing/selection bounds.
void listbox_draw(listbox_st *lb, const char * const *labels, int count);

// -1 if (x,y) isn't over a row, else the absolute row index (lb->top +
// on-screen offset).
int listbox_row_at(listbox_st *lb, int x, int y);

void listbox_clear_selection(listbox_st *lb);
void listbox_select_single(listbox_st *lb, int row);
void listbox_toggle(listbox_st *lb, int row);        // Ctrl-click
void listbox_range_select(listbox_st *lb, int row);  // Shift-click, from anchor_row

// Adjusts lb->top so `row` is on screen (e.g. after a fresh directory
// listing, or arrow-key navigation once that exists).
void listbox_ensure_visible(listbox_st *lb, int row);

#endif
