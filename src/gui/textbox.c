#include "../include/textbox.h"
#include "../include/video.h"
#include "../include/gui.h"
#include "../include/heap.h"

void textbox_clear(textbox_st *tb) {
	for (int r = 0; r < tb->rows; r++) {
		char *row = tb->cells + r * tb->stride;
		for (int c = 0; c < tb->cols; c++) row[c] = ' ';
		row[tb->cols] = '\0';
	}
	tb->cursor_row = 0;
	tb->cursor_col = 0;
}

void textbox_init(textbox_st *tb, int cols, int rows) {
	tb->cols = cols;
	tb->rows = rows;
	tb->stride = cols + 1;
	tb->cells = (char *)kmalloc(rows * tb->stride);
	textbox_clear(tb);
}

void textbox_free(textbox_st *tb) {
	kfree(tb->cells);
}

static void textbox_scroll(textbox_st *tb) {
	for (int r = 0; r < tb->rows - 1; r++) {
		char *dst = tb->cells + r * tb->stride;
		char *src = tb->cells + (r + 1) * tb->stride;
		for (int c = 0; c < tb->cols; c++) dst[c] = src[c];
	}
	char *last = tb->cells + (tb->rows - 1) * tb->stride;
	for (int c = 0; c < tb->cols; c++) last[c] = ' ';
}

static void textbox_newline(textbox_st *tb) {
	tb->cursor_col = 0;
	tb->cursor_row++;
	if (tb->cursor_row >= tb->rows) {
		textbox_scroll(tb);
		tb->cursor_row = tb->rows - 1;
	}
}

void textbox_putc(textbox_st *tb, char c) {
	if (c == '\n') {
		textbox_newline(tb);
		return;
	}
	if (c == '\b') {
		if (tb->cursor_col > 0) {
			tb->cursor_col--;
			tb->cells[tb->cursor_row * tb->stride + tb->cursor_col] = ' ';
		}
		return;
	}
	if (tb->cursor_col >= tb->cols) {
		textbox_newline(tb);
	}
	tb->cells[tb->cursor_row * tb->stride + tb->cursor_col] = c;
	tb->cursor_col++;
}

void textbox_puts(textbox_st *tb, const char *s) {
	while (s && *s) textbox_putc(tb, *s++);
}

void textbox_draw(textbox_st *tb, int x, int y, int show_cursor) {
	for (int r = 0; r < tb->rows; r++) {
		draw_text(x, y + r * FONT_H, tb->cells + r * tb->stride, GUI_COLOR_LIGHT);
	}
	if (show_cursor) {
		fill_rect(x + tb->cursor_col * FONT_W, y + tb->cursor_row * FONT_H + FONT_H - 1, FONT_W, 1, GUI_COLOR_LIGHT);
	}
}
