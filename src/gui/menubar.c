#include "../include/menubar.h"
#include "../include/video.h"
#include "../include/gui.h"
#include "../include/string.h"
#include "../include/widget.h"

#define MENUBAR_LABEL_PAD 16

void menubar_init(menubar_st *mb, rect_st rect, const menu_entry_t *entries, int entry_count) {
	mb->rect = rect;
	mb->entries = entries;
	mb->entry_count = entry_count;
	mb->open_index = -1;
	mb->hover_item = -1;
}

static int menubar_top_w(menubar_st *mb, int index) {
	return strlen((char *)mb->entries[index].label) * FONT_W + MENUBAR_LABEL_PAD;
}

static int menubar_top_x(menubar_st *mb, int index) {
	int x = mb->rect.x;
	for (int i = 0; i < index; i++) {
		x += menubar_top_w(mb, i);
	}
	return x;
}

static rect_st menubar_dropdown_rect(menubar_st *mb) {
	const menu_entry_t *e = &mb->entries[mb->open_index];
	int max_len = 0;
	for (int i = 0; i < e->item_count; i++) {
		int l = strlen((char *)e->items[i].label);
		if (l > max_len) {
			max_len = l;
		}
	}
	int w = max_len * FONT_W + 16;
	int h = e->item_count * MENUBAR_ITEM_H;
	rect_st r = { menubar_top_x(mb, mb->open_index), mb->rect.y + mb->rect.h, w, h };

	// Clip to the owning window's content width (see menubar.h's note on
	// why a dropdown can't float past its own window).
	if (r.x + r.w > mb->rect.x + mb->rect.w) {
		r.x = mb->rect.x + mb->rect.w - r.w;
	}
	if (r.x < mb->rect.x) {
		r.x = mb->rect.x;
	}
	return r;
}

void menubar_draw_bar(menubar_st *mb) {
	fill_rect(mb->rect.x, mb->rect.y, mb->rect.w, mb->rect.h, GUI_COLOR_BG);
	fill_rect(mb->rect.x, mb->rect.y + mb->rect.h - 1, mb->rect.w, 1, GUI_COLOR_SHADOW);

	int x = mb->rect.x;
	for (int i = 0; i < mb->entry_count; i++) {
		int w = menubar_top_w(mb, i);
		unsigned char color = GUI_COLOR_BLACK;
		if (i == mb->open_index) {
			fill_rect(x, mb->rect.y, w, mb->rect.h, GUI_COLOR_TITLE);
			color = GUI_COLOR_TITLE_TEXT;
		}
		draw_text(x + MENUBAR_LABEL_PAD / 2, mb->rect.y + (mb->rect.h - FONT_H) / 2,
		          (char *)mb->entries[i].label, color);
		x += w;
	}
}

void menubar_draw_dropdown(menubar_st *mb) {
	if (mb->open_index < 0) {
		return;
	}
	const menu_entry_t *e = &mb->entries[mb->open_index];
	rect_st dd = menubar_dropdown_rect(mb);

	fill_rect(dd.x, dd.y, dd.w, dd.h, GUI_COLOR_BG);
	draw_bevel(dd.x, dd.y, dd.w, dd.h, 0);

	for (int i = 0; i < e->item_count; i++) {
		int iy = dd.y + i * MENUBAR_ITEM_H;
		unsigned char color = e->items[i].enabled ? GUI_COLOR_BLACK : GUI_COLOR_SHADOW;
		if (i == mb->hover_item && e->items[i].enabled) {
			fill_rect(dd.x, iy, dd.w, MENUBAR_ITEM_H, GUI_COLOR_TITLE);
			color = GUI_COLOR_TITLE_TEXT;
		}
		draw_text(dd.x + 4, iy + (MENUBAR_ITEM_H - FONT_H) / 2, (char *)e->items[i].label, color);
	}
}

void menubar_draw(menubar_st *mb) {
	menubar_draw_bar(mb);
	menubar_draw_dropdown(mb);
}

int menubar_hit_top(menubar_st *mb, int x, int y) {
	if (y < mb->rect.y || y >= mb->rect.y + mb->rect.h) {
		return -1;
	}
	int cx = mb->rect.x;
	for (int i = 0; i < mb->entry_count; i++) {
		int w = menubar_top_w(mb, i);
		if (x >= cx && x < cx + w) {
			return i;
		}
		cx += w;
	}
	return -1;
}

static int menubar_dropdown_row_at(menubar_st *mb, int x, int y) {
	if (mb->open_index < 0) {
		return -1;
	}
	rect_st dd = menubar_dropdown_rect(mb);
	if (!rect_contains_point(&dd, x, y)) {
		return -1;
	}
	const menu_entry_t *e = &mb->entries[mb->open_index];
	int row = (y - dd.y) / MENUBAR_ITEM_H;
	if (row < 0 || row >= e->item_count) {
		return -1;
	}
	return row;
}

void menubar_track_hover(menubar_st *mb, int x, int y) {
	mb->hover_item = menubar_dropdown_row_at(mb, x, y);
}

int menubar_hit_item(menubar_st *mb, int x, int y) {
	int row = menubar_dropdown_row_at(mb, x, y);
	if (row < 0) {
		return -1;
	}
	const menu_entry_t *e = &mb->entries[mb->open_index];
	if (!e->items[row].enabled) {
		return -1;
	}
	return e->items[row].id;
}

void menubar_open(menubar_st *mb, int index) {
	mb->open_index = index;
	mb->hover_item = -1;
}

void menubar_close(menubar_st *mb) {
	mb->open_index = -1;
	mb->hover_item = -1;
}
