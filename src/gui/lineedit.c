#include "../include/lineedit.h"
#include "../include/video.h"
#include "../include/gui.h"
#include "../include/string.h"
#include "../include/widget.h"

void lineedit_open(lineedit_st *le, const char *prompt) {
	le->text[0] = 0;
	le->len = 0;
	le->active = 1;
	le->prompt = prompt;
}

void lineedit_close(lineedit_st *le) {
	le->active = 0;
}

int lineedit_key(lineedit_st *le, int ascii) {
	if (ascii == '\n') {
		return 1;
	}
	if (ascii == 27) { // Esc
		lineedit_close(le);
		return -1;
	}
	if (ascii == '\b') {
		if (le->len > 0) {
			le->len--;
			le->text[le->len] = 0;
		}
		return 0;
	}
	if (ascii >= 32 && ascii < 127 && le->len < LINEEDIT_MAX - 1) {
		le->text[le->len] = (char)ascii;
		le->len++;
		le->text[le->len] = 0;
	}
	return 0;
}

void lineedit_draw(lineedit_st *le, rect_st content) {
	int h = 24;
	int x = content.x;
	int y = content.y + content.h - h;
	int w = content.w;

	fill_rect(x, y, w, h, GUI_COLOR_BG);
	draw_bevel(x, y, w, h, 1); // sunken: reads as an inset field

	int tx = x + 4;
	int ty = y + (h - FONT_H) / 2;
	if (le->prompt) {
		draw_text(tx, ty, (char *)le->prompt, GUI_COLOR_BLACK);
		tx += (strlen((char *)le->prompt) + 1) * FONT_W;
	}
	draw_text(tx, ty, le->text, GUI_COLOR_BLACK);
	fill_rect(tx + le->len * FONT_W, ty, FONT_W, FONT_H, GUI_COLOR_BLACK);
}
