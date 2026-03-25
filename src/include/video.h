#ifndef _VIDEO_H
#define _VIDEO_H

int get_cursor_pos();
void rm_char_in_pos(int);

void clear_screen();
void next_line();

void kernel_print_text(char*, char);
void kernel_print_char(char, char);

void scroll();

// Pixel/graphics helpers for Mode 13h (320x200x8bpp)
void draw_pixel(int x, int y, unsigned char color);
void fill_rect(int x, int y, int w, int h, unsigned char color);
void draw_rect(int x, int y, int w, int h, unsigned char color);
void draw_text(int x, int y, char* text, unsigned char color);

// Port I/O helpers used by the keyboard driver.
unsigned char port_byte_read(unsigned short port);
void port_byte_write(unsigned short port, unsigned char data);

#endif  