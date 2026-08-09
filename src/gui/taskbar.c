#include "../include/taskbar.h"
#include "../include/video.h"
#include "../include/gui.h"
#include "../include/string.h"
#include "../include/widget.h"

#define TASKBAR_MARGIN       2
#define TASKBAR_START_PAD    12
#define TASKBAR_BUTTON_GAP   4
#define TASKBAR_BUTTON_MIN_W 40
#define TASKBAR_BUTTON_PAD   8
#define TASKBAR_LABEL_PAD    16

rect_st taskbar_bar_rect(void) {
	rect_st r = { 0, SCREEN_H - TASKBAR_HEIGHT, SCREEN_W, TASKBAR_HEIGHT };
	return r;
}

rect_st taskbar_start_button_rect(void) {
	rect_st bar = taskbar_bar_rect();
	rect_st r;
	r.w = strlen("Iniciar") * FONT_W + TASKBAR_START_PAD;
	r.h = TASKBAR_HEIGHT - 2 * TASKBAR_MARGIN;
	r.x = TASKBAR_MARGIN;
	r.y = bar.y + TASKBAR_MARGIN;
	return r;
}

static int taskbar_button_w(int button_count) {
	if (button_count <= 0) {
		return 0;
	}
	rect_st start = taskbar_start_button_rect();
	int x0 = start.x + start.w + TASKBAR_BUTTON_GAP;
	int avail = SCREEN_W - x0 - TASKBAR_MARGIN;
	int w = avail / button_count;
	if (w < TASKBAR_BUTTON_MIN_W) {
		w = TASKBAR_BUTTON_MIN_W;
	}
	return w;
}

static rect_st taskbar_button_rect_at(int index, int button_count) {
	rect_st bar = taskbar_bar_rect();
	rect_st start = taskbar_start_button_rect();
	int w = taskbar_button_w(button_count);
	rect_st r;
	r.w = w;
	r.h = TASKBAR_HEIGHT - 2 * TASKBAR_MARGIN;
	r.x = start.x + start.w + TASKBAR_BUTTON_GAP + index * w;
	r.y = bar.y + TASKBAR_MARGIN;
	return r;
}

// Copies up to (rect width / FONT_W) characters of `title` into `out`
// (caller-sized buffer), no ellipsis -- accepted simplification, matches
// the project's habit of skipping cosmetic-only infrastructure.
static void taskbar_truncate(char *out, int out_size, const char *title, int button_w) {
	int max_chars = (button_w - TASKBAR_BUTTON_PAD) / FONT_W;
	if (max_chars > out_size - 1) {
		max_chars = out_size - 1;
	}
	if (max_chars < 0) {
		max_chars = 0;
	}
	int i = 0;
	while (i < max_chars && title[i]) {
		out[i] = title[i];
		i++;
	}
	out[i] = 0;
}

static void taskbar_draw_button(rect_st r, const char *label, unsigned char face, unsigned char text_color) {
	fill_rect(r.x, r.y, r.w, r.h, face);
	draw_bevel(r.x, r.y, r.w, r.h, 0);

	draw_text(r.x + TASKBAR_BUTTON_PAD / 2, r.y + (r.h - FONT_H) / 2, (char *)label, text_color);
}

void taskbar_draw_bar(int start_open, const taskbar_button_t *buttons, int button_count) {
	rect_st bar = taskbar_bar_rect();
	fill_rect(bar.x, bar.y, bar.w, bar.h, GUI_COLOR_BG);
	fill_rect(bar.x, bar.y, bar.w, 1, GUI_COLOR_LIGHT);

	rect_st start = taskbar_start_button_rect();
	taskbar_draw_button(start, "Iniciar", start_open ? GUI_COLOR_TITLE : GUI_COLOR_BG,
	                     start_open ? GUI_COLOR_TITLE_TEXT : GUI_COLOR_BLACK);

	for (int i = 0; i < button_count; i++) {
		rect_st r = taskbar_button_rect_at(i, button_count);
		char label[24];
		taskbar_truncate(label, sizeof(label), buttons[i].title, r.w);

		unsigned char face = buttons[i].focused ? GUI_COLOR_TITLE : GUI_COLOR_BG;
		unsigned char text_color;
		if (buttons[i].focused) {
			text_color = GUI_COLOR_TITLE_TEXT;
		} else if (buttons[i].minimized) {
			text_color = GUI_COLOR_SHADOW;
		} else {
			text_color = GUI_COLOR_BLACK;
		}

		taskbar_draw_button(r, label, face, text_color);
	}
}

static rect_st taskbar_start_menu_rect(const char *const *labels, int label_count) {
	int max_len = 0;
	for (int i = 0; i < label_count; i++) {
		int l = strlen((char *)labels[i]);
		if (l > max_len) {
			max_len = l;
		}
	}
	rect_st start = taskbar_start_button_rect();
	rect_st bar = taskbar_bar_rect();
	rect_st r;
	r.w = max_len * FONT_W + TASKBAR_LABEL_PAD;
	r.h = label_count * TASKBAR_ITEM_H;
	r.x = start.x;
	r.y = bar.y - r.h;

	if (r.x + r.w > SCREEN_W) {
		r.x = SCREEN_W - r.w;
	}
	if (r.x < 0) {
		r.x = 0;
	}
	return r;
}

void taskbar_draw_start_menu(const char *const *labels, int label_count, int hover_index) {
	rect_st dd = taskbar_start_menu_rect(labels, label_count);

	fill_rect(dd.x, dd.y, dd.w, dd.h, GUI_COLOR_BG);
	draw_bevel(dd.x, dd.y, dd.w, dd.h, 0);

	for (int i = 0; i < label_count; i++) {
		int iy = dd.y + i * TASKBAR_ITEM_H;
		unsigned char color = GUI_COLOR_BLACK;
		if (i == hover_index) {
			fill_rect(dd.x, iy, dd.w, TASKBAR_ITEM_H, GUI_COLOR_TITLE);
			color = GUI_COLOR_TITLE_TEXT;
		}
		draw_text(dd.x + 4, iy + (TASKBAR_ITEM_H - FONT_H) / 2, (char *)labels[i], color);
	}
}

int taskbar_hit_button(int button_count, int x, int y) {
	for (int i = 0; i < button_count; i++) {
		rect_st r = taskbar_button_rect_at(i, button_count);
		if (rect_contains_point(&r, x, y)) {
			return i;
		}
	}
	return -1;
}

int taskbar_hit_start_item(const char *const *labels, int label_count, int x, int y) {
	rect_st dd = taskbar_start_menu_rect(labels, label_count);
	if (!rect_contains_point(&dd, x, y)) {
		return -1;
	}
	int row = (y - dd.y) / TASKBAR_ITEM_H;
	if (row < 0 || row >= label_count) {
		return -1;
	}
	return row;
}
