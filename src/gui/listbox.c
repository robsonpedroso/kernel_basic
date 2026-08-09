#include "../include/listbox.h"
#include "../include/video.h"
#include "../include/gui.h"

static int listbox_visible_rows(listbox_st *lb) {
	int n = lb->rect.h / FONT_H;
	return n > 0 ? n : 1;
}

void listbox_init(listbox_st *lb, rect_st rect) {
	lb->rect = rect;
	lb->count = 0;
	lb->top = 0;
	lb->anchor_row = -1;
	for (int i = 0; i < LISTBOX_MAX_ROWS; i++) {
		lb->selected[i] = 0;
	}
}

void listbox_draw(listbox_st *lb, const char * const *labels, int count) {
	lb->count = count;
	fill_rect(lb->rect.x, lb->rect.y, lb->rect.w, lb->rect.h, GUI_COLOR_BLACK);

	int visible = listbox_visible_rows(lb);
	for (int i = 0; i < visible; i++) {
		int row = lb->top + i;
		if (row >= count) {
			break;
		}
		int ry = lb->rect.y + i * FONT_H;
		unsigned char color = GUI_COLOR_LIGHT;
		if (lb->selected[row]) {
			fill_rect(lb->rect.x, ry, lb->rect.w, FONT_H, GUI_COLOR_TITLE);
			color = GUI_COLOR_TITLE_TEXT;
		}
		draw_text(lb->rect.x + 2, ry, (char *)labels[row], color);
	}
}

int listbox_row_at(listbox_st *lb, int x, int y) {
	if (!rect_contains_point(&lb->rect, x, y)) {
		return -1;
	}
	int rel = (y - lb->rect.y) / FONT_H;
	int row = lb->top + rel;
	if (row < 0 || row >= lb->count) {
		return -1;
	}
	return row;
}

void listbox_clear_selection(listbox_st *lb) {
	for (int i = 0; i < LISTBOX_MAX_ROWS; i++) {
		lb->selected[i] = 0;
	}
}

void listbox_select_single(listbox_st *lb, int row) {
	listbox_clear_selection(lb);
	if (row >= 0 && row < lb->count) {
		lb->selected[row] = 1;
		lb->anchor_row = row;
	}
}

void listbox_toggle(listbox_st *lb, int row) {
	if (row < 0 || row >= lb->count) {
		return;
	}
	lb->selected[row] = (unsigned char)!lb->selected[row];
	lb->anchor_row = row;
}

void listbox_range_select(listbox_st *lb, int row) {
	if (row < 0 || row >= lb->count) {
		return;
	}
	if (lb->anchor_row < 0) {
		listbox_select_single(lb, row);
		return;
	}
	// Anchor stays put across repeated Shift-clicks (it's the range start,
	// not "last clicked") so the selection can grow/shrink from one end.
	int lo = lb->anchor_row < row ? lb->anchor_row : row;
	int hi = lb->anchor_row < row ? row : lb->anchor_row;
	listbox_clear_selection(lb);
	for (int i = lo; i <= hi; i++) {
		lb->selected[i] = 1;
	}
}

void listbox_ensure_visible(listbox_st *lb, int row) {
	if (row < 0) {
		return;
	}
	int visible = listbox_visible_rows(lb);
	if (row < lb->top) {
		lb->top = row;
	} else if (row >= lb->top + visible) {
		lb->top = row - visible + 1;
	}
	if (lb->top < 0) {
		lb->top = 0;
	}
}
