#ifndef _SPLITTER_H
#define _SPLITTER_H

#include "rect.h"

#define SPLITTER_WIDTH 4

// Deliberately minimal, no owned struct: the File Manager keeps its own
// int split_x, matching wm.c's own pattern of holding drag state locally
// rather than in a shared struct. Dragging needs no new wm.c plumbing --
// wm_on_mouse_down already puts the WM into WM_APP_CAPTURED for the whole
// gesture once a content click lands anywhere, including on this bar.
rect_st splitter_bar_rect(rect_st pane_area, int split_x);
void    splitter_draw(rect_st bar);
int     splitter_hit_test(rect_st bar, int x, int y);

#endif
