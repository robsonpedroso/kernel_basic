#include "../include/widget.h"
#include "../include/video.h"
#include "../include/gui.h"
#include "../include/string.h"

// Classic "chiseled" 3D bevel: light edge on top/left, dark edge on
// bottom/right reads as raised; swapping them reads as pressed/sunken.
void draw_bevel(int x, int y, int w, int h, int pressed) {
	unsigned char top_left = pressed ? GUI_COLOR_SHADOW : GUI_COLOR_LIGHT;
	unsigned char bottom_right = pressed ? GUI_COLOR_LIGHT : GUI_COLOR_SHADOW;

	fill_rect(x, y, w, 1, top_left);
	fill_rect(x, y, 1, h, top_left);
	fill_rect(x, y + h - 1, w, 1, bottom_right);
	fill_rect(x + w - 1, y, 1, h, bottom_right);
}

void widget_button_draw(widget_st *w) {
	int x = w->rect.x, y = w->rect.y, width = w->rect.w, height = w->rect.h;

	fill_rect(x, y, width, height, GUI_COLOR_BG);
	draw_bevel(x, y, width, height, w->pressed);

	if (w->selected) {
		draw_rect(x + 3, y + 3, width - 6, height - 6, GUI_COLOR_BLACK);
	}

	int text_len = strlen((char *)w->label);
	int text_width = text_len * FONT_W;
	int offset = w->pressed ? 1 : 0;
	int tx = x + (width / 2) - (text_width / 2) + offset;
	int ty = y + (height / 2) - (FONT_H / 2) + offset;

	draw_text(tx, ty, (char *)w->label, GUI_COLOR_BLACK);
}
