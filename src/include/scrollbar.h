#ifndef _SCROLLBAR_H
#define _SCROLLBAR_H

#include "rect.h"

// Vertical only -- matches this codebase's already-accepted "no horizontal
// scroll" precedent (see editbuf.h). No arrow buttons, just track + thumb:
// drag the thumb, or click the track to page.
typedef struct scrollbar {
	rect_st rect;   // content-relative, the full vertical bar
	int total;      // total scrollable units (e.g. row count)
	int visible;    // units visible at once
	int pos;        // first visible unit
	int dragging;
} scrollbar_st;

void scrollbar_draw(scrollbar_st *sb);

// -1 = not on the bar at all, 0 = on the track (caller may page by
// ->visible), 1 = on the thumb (caller should set sb->dragging = 1 and
// route subsequent on_mouse_move calls to scrollbar_drag_to).
int scrollbar_hit_test(scrollbar_st *sb, int x, int y);

// Called from the owner's on_mouse_move while sb->dragging is set; updates
// sb->pos from the thumb's new y position, clamped to [0, total-visible].
void scrollbar_drag_to(scrollbar_st *sb, int y);

#endif
