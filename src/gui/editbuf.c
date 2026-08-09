#include "../include/editbuf.h"
#include "../include/heap.h"
#include "../include/keyboard.h"

void editbuf_init(editbuf_st *eb, int capacity) {
	eb->data = (char *)kmalloc((unsigned int)capacity);
	eb->cap = capacity;
	eb->len = 0;
	eb->cursor = 0;
	eb->goal_col = -1;
	eb->top_line = 0;
	eb->dirty = 0;
}

void editbuf_free(editbuf_st *eb) {
	kfree(eb->data);
}

void editbuf_load(editbuf_st *eb, const char *src, int len) {
	if (len > eb->cap) {
		len = eb->cap;
	}
	for (int i = 0; i < len; i++) {
		eb->data[i] = src[i];
	}
	eb->len = len;
	eb->cursor = 0;
	eb->goal_col = -1;
	eb->top_line = 0;
	eb->dirty = 0;
}

static int line_start(editbuf_st *eb, int off) {
	int i = off;
	while (i > 0 && eb->data[i - 1] != '\n') {
		i--;
	}
	return i;
}

static int line_end(editbuf_st *eb, int off) {
	int i = off;
	while (i < eb->len && eb->data[i] != '\n') {
		i++;
	}
	return i;
}

static void eb_insert(editbuf_st *eb, char c) {
	if (eb->len >= eb->cap) {
		return;
	}
	for (int i = eb->len; i > eb->cursor; i--) {
		eb->data[i] = eb->data[i - 1];
	}
	eb->data[eb->cursor] = c;
	eb->cursor++;
	eb->len++;
}

static void eb_erase(editbuf_st *eb, int at) {
	for (int i = at; i < eb->len - 1; i++) {
		eb->data[i] = eb->data[i + 1];
	}
	eb->len--;
}

static void eb_up(editbuf_st *eb) {
	if (eb->goal_col < 0) {
		eb->goal_col = eb->cursor - line_start(eb, eb->cursor);
	}
	int ls = line_start(eb, eb->cursor);
	if (ls == 0) {
		return; // already on the first line
	}
	int prev_start = line_start(eb, ls - 1); // ls-1 is the '\n' ending the previous line
	int prev_len = (ls - 1) - prev_start;
	eb->cursor = prev_start + (eb->goal_col < prev_len ? eb->goal_col : prev_len);
}

static void eb_down(editbuf_st *eb) {
	if (eb->goal_col < 0) {
		eb->goal_col = eb->cursor - line_start(eb, eb->cursor);
	}
	int le = line_end(eb, eb->cursor);
	if (le >= eb->len) {
		return; // already on the last line
	}
	int next_start = le + 1;
	int next_len = line_end(eb, next_start) - next_start;
	eb->cursor = next_start + (eb->goal_col < next_len ? eb->goal_col : next_len);
}

int editbuf_key(editbuf_st *eb, int ascii) {
	switch (ascii) {
		case KEY_LEFT:
			if (eb->cursor > 0) {
				eb->cursor--;
			}
			eb->goal_col = -1;
			return 0;
		case KEY_RIGHT:
			if (eb->cursor < eb->len) {
				eb->cursor++;
			}
			eb->goal_col = -1;
			return 0;
		case KEY_UP:
			eb_up(eb);
			return 0;
		case KEY_DOWN:
			eb_down(eb);
			return 0;
		case KEY_HOME:
			eb->cursor = line_start(eb, eb->cursor);
			eb->goal_col = -1;
			return 0;
		case KEY_END:
			eb->cursor = line_end(eb, eb->cursor);
			eb->goal_col = -1;
			return 0;
		case KEY_DELETE:
			eb->goal_col = -1;
			if (eb->cursor < eb->len) {
				eb_erase(eb, eb->cursor);
				eb->dirty = 1;
				return 1;
			}
			return 0;
		case '\b':
			eb->goal_col = -1;
			if (eb->cursor > 0) {
				eb->cursor--;
				eb_erase(eb, eb->cursor);
				eb->dirty = 1;
				return 1;
			}
			return 0;
		case '\n':
			eb_insert(eb, '\n');
			eb->dirty = 1;
			eb->goal_col = -1;
			return 1;
		case '\t':
			// No tab support in a fixed 8x8-font grid -- pad with spaces.
			eb_insert(eb, ' ');
			eb_insert(eb, ' ');
			eb_insert(eb, ' ');
			eb_insert(eb, ' ');
			eb->dirty = 1;
			eb->goal_col = -1;
			return 1;
		default:
			if (ascii >= 32 && ascii < 127) {
				eb_insert(eb, (char)ascii);
				eb->dirty = 1;
				eb->goal_col = -1;
				return 1;
			}
			return 0;
	}
}

void editbuf_render(editbuf_st *eb, textbox_st *tb) {
	// Single forward scan to find the cursor's (line, col) -- fine at this
	// scale (a few KB, once per composite).
	int cur_line = 0;
	int line_begin = 0;
	for (int i = 0; i < eb->cursor; i++) {
		if (eb->data[i] == '\n') {
			cur_line++;
			line_begin = i + 1;
		}
	}
	int cur_col = eb->cursor - line_begin;

	if (cur_line < eb->top_line) {
		eb->top_line = cur_line;
	}
	if (cur_line >= eb->top_line + tb->rows) {
		eb->top_line = cur_line - tb->rows + 1;
	}
	if (eb->top_line < 0) {
		eb->top_line = 0;
	}

	textbox_clear(tb);

	// Byte offset where line `top_line` starts.
	int offset = 0;
	int line = 0;
	while (line < eb->top_line && offset < eb->len) {
		if (eb->data[offset] == '\n') {
			line++;
		}
		offset++;
	}

	int row = 0, col = 0;
	while (offset < eb->len && row < tb->rows) {
		char c = eb->data[offset];
		if (c == '\n') {
			row++;
			col = 0;
		} else {
			if (col < tb->cols) {
				tb->cells[row * tb->stride + col] = c;
			}
			col++;
		}
		offset++;
	}

	tb->cursor_row = cur_line - eb->top_line;
	tb->cursor_col = (cur_col < tb->cols) ? cur_col : tb->cols - 1;
}
