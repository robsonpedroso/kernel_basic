#ifndef _VIDEO_H
#define _VIDEO_H

// Font dimensions
#define FONT_W 8
#define FONT_H 8

// Screen resolution (Mode 12h, 640x480, 16-color planar VGA). Shared with
// mouse.c so the cursor can be clamped to the visible area.
#define SCREEN_W 640
#define SCREEN_H 480

int get_cursor_pos();
void rm_char_in_pos(int);

void clear_screen();
void next_line();

void kernel_print_text(char*, char);
void kernel_print_char(char, char);

void scroll();

// Pixel/graphics helpers for Mode 12h (640x480, 16-color planar VGA).
// Colors are EGA/VGA palette indices 0-15.
void draw_pixel(int x, int y, unsigned char color);
unsigned char get_pixel(int x, int y);
void fill_rect(int x, int y, int w, int h, unsigned char color);
void draw_rect(int x, int y, int w, int h, unsigned char color);
void draw_text(int x, int y, char* text, unsigned char color);

#endif