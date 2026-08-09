#include "../include/window.h"
#include "../include/video.h"
#include "../include/gui.h"
#include "../include/string.h"
#include "../include/widget.h"

rect_st window_sysmenu_rect(window_st *win) {
	rect_st r;
	r.w = WINDOW_BOX_SIZE;
	r.h = WINDOW_BOX_SIZE;
	r.x = win->rect.x + WINDOW_BORDER + 2;
	r.y = win->rect.y + WINDOW_BORDER + (WINDOW_TITLE_HEIGHT - WINDOW_BOX_SIZE) / 2;
	return r;
}

rect_st window_maximize_rect(window_st *win) {
	rect_st r;
	r.w = WINDOW_BOX_SIZE;
	r.h = WINDOW_BOX_SIZE;
	r.x = win->rect.x + win->rect.w - WINDOW_BORDER - 2 - WINDOW_BOX_SIZE;
	r.y = win->rect.y + WINDOW_BORDER + (WINDOW_TITLE_HEIGHT - WINDOW_BOX_SIZE) / 2;
	return r;
}

rect_st window_minimize_rect(window_st *win) {
	rect_st r = window_maximize_rect(win);
	r.x -= (WINDOW_BOX_SIZE + 2);
	return r;
}

static void draw_title_box(rect_st r, char glyph) {
	fill_rect(r.x, r.y, r.w, r.h, GUI_COLOR_BG);
	draw_bevel(r.x, r.y, r.w, r.h, 0);

	char text[2] = { glyph, 0 };
	draw_text(r.x + (r.w - FONT_W) / 2, r.y + (r.h - FONT_H) / 2, text, GUI_COLOR_BLACK);
}

void window_draw(window_st *win, int active) {
	int x = win->rect.x, y = win->rect.y, w = win->rect.w, h = win->rect.h;

	// Outer black outline, then a raised 3D bevel just inside it -- the
	// classic Win3.11 window frame.
	draw_rect(x, y, w, h, GUI_COLOR_BLACK);
	draw_bevel(x + 1, y + 1, w - 2, h - 2, 0);

	rect_st content = window_content_rect(win);
	fill_rect(content.x, content.y, content.w, content.h, GUI_COLOR_BG);

	unsigned char title_color = active ? GUI_COLOR_TITLE : GUI_COLOR_SHADOW;
	fill_rect(x + WINDOW_BORDER, y + WINDOW_BORDER, w - 2 * WINDOW_BORDER, WINDOW_TITLE_HEIGHT, title_color);

	rect_st sysmenu = window_sysmenu_rect(win);
	rect_st minbox = window_minimize_rect(win);
	rect_st maxbox = window_maximize_rect(win);
	draw_title_box(sysmenu, '-');
	draw_title_box(minbox, '_');
	draw_title_box(maxbox, '^');

	unsigned char text_color = active ? GUI_COLOR_TITLE_TEXT : GUI_COLOR_BLACK;
	int title_x = sysmenu.x + sysmenu.w + 6;
	int title_y = y + WINDOW_BORDER + (WINDOW_TITLE_HEIGHT - FONT_H) / 2;
	draw_text(title_x, title_y, (char *)win->title, text_color);
}

rect_st window_content_rect(window_st *win) {
	rect_st r;
	r.x = win->rect.x + WINDOW_BORDER;
	r.y = win->rect.y + WINDOW_BORDER + WINDOW_TITLE_HEIGHT;
	r.w = win->rect.w - 2 * WINDOW_BORDER;
	r.h = win->rect.h - 2 * WINDOW_BORDER - WINDOW_TITLE_HEIGHT;
	return r;
}
