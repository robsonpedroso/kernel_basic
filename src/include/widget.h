#ifndef _WIDGET_H
#define _WIDGET_H

#include "rect.h"

typedef struct widget {
	rect_st rect;
	const char *label;
	int selected; // keyboard focus or mouse hover
	int pressed;  // mouse button currently held down on it (sunken bevel)
} widget_st;

void widget_button_draw(widget_st *w);

// Classic "chiseled" 3D bevel: light edge on top/left, dark edge on
// bottom/right reads as raised (pressed=0); swapping them reads as
// pressed/sunken (pressed=1). Shared by every widget/app that draws a
// raised or sunken box border.
void draw_bevel(int x, int y, int w, int h, int pressed);

#endif
