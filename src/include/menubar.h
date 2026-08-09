#ifndef _MENUBAR_H
#define _MENUBAR_H

#include "rect.h"

#define MENUBAR_HEIGHT  16
#define MENUBAR_ITEM_H  16

typedef struct menu_item {
	const char *label;
	int id;        // caller-defined command id, returned by menubar_hit_item
	int enabled;
} menu_item_t;

typedef struct menu_entry { // one top-level "File"/"Disk"/... slot
	const char *label;
	const menu_item_t *items; // static const table owned by the caller
	int item_count;
} menu_entry_t;

// Limitation, stated rather than hidden: this GUI has no cross-window
// compositing layer (wm.c's wm_composite draws each window's content
// independently), so a dropdown here is confined to its own window's
// content rect -- it cannot float above sibling windows the way a real
// Win3.11 menu can. Same reason lineedit_st's prompt is drawn inside the
// content area instead of as a floating dialog.
typedef struct menubar {
	rect_st rect;              // content-relative, spans the full window width
	const menu_entry_t *entries;
	int entry_count;
	int open_index;            // -1 = closed, else which top-level menu is dropped down
	int hover_item;            // index into the open entry's items[], -1 = none
} menubar_st;

void menubar_init(menubar_st *mb, rect_st rect, const menu_entry_t *entries, int entry_count);

// Split in two so a caller that draws other content below the bar (drive
// bar, panes, status bar) can call _draw_bar first, then that content, then
// _draw_dropdown last -- otherwise the dropdown (which extends below the
// bar, over that content) would get immediately painted over. menubar_draw
// is the old single-call convenience for callers with nothing below the bar.
void menubar_draw_bar(menubar_st *mb);
void menubar_draw_dropdown(menubar_st *mb); // no-op if mb->open_index < 0
void menubar_draw(menubar_st *mb);          // == _draw_bar then _draw_dropdown

// -1 if (x,y) isn't over any top-level label, else its index.
int menubar_hit_top(menubar_st *mb, int x, int y);

// Valid only while mb->open_index >= 0: -1 if (x,y) isn't over an enabled
// dropdown item, else that item's caller-defined id.
int menubar_hit_item(menubar_st *mb, int x, int y);

// Updates mb->hover_item for the highlight, without acting on a click --
// call from on_mouse_move while a dropdown is open.
void menubar_track_hover(menubar_st *mb, int x, int y);

void menubar_open(menubar_st *mb, int index);
void menubar_close(menubar_st *mb);

#endif
