#ifndef _TEXTBOX_H
#define _TEXTBOX_H

// Small, fixed-size character-grid console widget. Not a growing
// scrollback (no realloc exists) -- caller picks cols/rows once at init
// time. Used by windowed apps that need their own text output area,
// instead of the old fullscreen singleton console in video.c.
typedef struct textbox {
	int cols, rows;
	int stride;   // cols + 1; column [cols] of every row is always '\0',
	              // so each row is already a valid C string for draw_text.
	char *cells;  // kmalloc'd, rows * stride bytes, row-major
	int cursor_row, cursor_col;
} textbox_st;

void textbox_init(textbox_st *tb, int cols, int rows);
void textbox_free(textbox_st *tb);
void textbox_clear(textbox_st *tb);

// Handles '\n' (newline+scroll), '\b' (backspace), and printable chars
// (auto-wraps at end of row).
void textbox_putc(textbox_st *tb, char c);
void textbox_puts(textbox_st *tb, const char *s);

// x,y = screen pixel coords of the grid's top-left corner.
void textbox_draw(textbox_st *tb, int x, int y, int show_cursor);

#endif
