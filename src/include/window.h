#ifndef _WINDOW_H
#define _WINDOW_H

#include "rect.h"

#define WINDOW_TITLE_HEIGHT 20
#define WINDOW_BORDER 3
#define WINDOW_BOX_SIZE 16

typedef struct window {
	rect_st rect;
	const char *title;
} window_st;

// active: title bar drawn in the "focused" color (blue) vs "unfocused"
// (gray) -- an authentic Win3.11 detail.
void window_draw(window_st *win, int active);

// Area inside the border and below the title bar, where callers draw
// their own content (buttons, text, ...).
rect_st window_content_rect(window_st *win);

// Title-bar chrome hit-boxes, in screen coordinates. window.c uses these
// itself to draw the boxes; wm.c uses the same functions for hit-testing,
// so layout only lives in one place.
rect_st window_sysmenu_rect(window_st *win);
rect_st window_minimize_rect(window_st *win);
rect_st window_maximize_rect(window_st *win);

#endif
