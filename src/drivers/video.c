
#include "../include/video.h"
#include "../include/font8x8_basic.h"

// Keep these functions available: keyboard.c uses port I/O.
unsigned char port_byte_read(unsigned short port) {
	unsigned char result;
	__asm__ ( "in %%dx,%%al":"=a"( result ):"d"( port ));
	return result;
}

void port_byte_write(unsigned short port, unsigned char data) {
	__asm__ ("out %%al,%%dx" : : "a"( data ) ,"d"( port ));
}

// Reverse bits in a byte (for font rendering)
static unsigned char reverse_bits(unsigned char byte) {
	unsigned char result = 0;
	for (int i = 0; i < 8; i++) {
		result = (result << 1) | (byte & 1);
		byte >>= 1;
	}
	return result;
}

// Mode 13h: 320x200, 1 byte per pixel (palette index)
#define FRAMEBUFFER 0xA0000
#define SCREEN_W 320
#define SCREEN_H 200

// Simple 8x8 font rendering. Text grid becomes 40 cols x 25 rows.
#define TEXT_COLS (SCREEN_W / FONT_W)  // 40
#define TEXT_ROWS (SCREEN_H / FONT_H)  // 25

static unsigned char bg_color = 0;
static int cursor_pos = 0; // in "text cells" (not pixels)

static void draw_char_at_pixel(int x, int y, char c, unsigned char fg) {
	unsigned char uc = (unsigned char)c;
	if (uc >= 128) uc = '?';

	unsigned char* fb = (unsigned char*)FRAMEBUFFER;
	for (int row = 0; row < FONT_H; row++) {
		unsigned char bits = reverse_bits(font8x8_basic[uc][row]);
		for (int col = 0; col < FONT_W; col++) {
			if (bits & (1 << (7 - col))) {
				int px = x + col;
				int py = y + row;
				if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
					fb[py * SCREEN_W + px] = fg;
				}
			}
		}
	}
}

int get_cursor_pos() {
	return cursor_pos;
}

void draw_pixel(int x, int y, unsigned char color) {
	if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
	((unsigned char*)FRAMEBUFFER)[y * SCREEN_W + x] = color;
}

void fill_rect(int x, int y, int w, int h, unsigned char color) {
	if (w <= 0 || h <= 0) return;
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > SCREEN_W) w = SCREEN_W - x;
	if (y + h > SCREEN_H) h = SCREEN_H - y;
	if (w <= 0 || h <= 0) return;

	unsigned char* fb = (unsigned char*)FRAMEBUFFER;
	for (int yy = 0; yy < h; yy++) {
		for (int xx = 0; xx < w; xx++) {
			fb[(y + yy) * SCREEN_W + (x + xx)] = color;
		}
	}
}

void draw_rect(int x, int y, int w, int h, unsigned char color) {
	if (w <= 0 || h <= 0) return;
	fill_rect(x, y, w, 1, color);
	fill_rect(x, y + h - 1, w, 1, color);
	fill_rect(x, y, 1, h, color);
	fill_rect(x + w - 1, y, 1, h, color);
}

void draw_text(int x, int y, char* text, unsigned char color) {
	// x/y are pixel coordinates; text is rendered with 8x8 glyphs.
	while (text && *text) {
		draw_char_at_pixel(x, y, *text, color);
		x += FONT_W;
		text++;
	}
}

void clear_screen() {
	fill_rect(0, 0, SCREEN_W, SCREEN_H, bg_color);
	cursor_pos = 0;
}

void scroll() {
	// Scroll by one text row (8 pixels).
	unsigned char* fb = (unsigned char*)FRAMEBUFFER;
	for (int y = FONT_H; y < SCREEN_H; y++) {
		for (int x = 0; x < SCREEN_W; x++) {
			fb[(y - FONT_H) * SCREEN_W + x] = fb[y * SCREEN_W + x];
		}
	}
	// Clear bottom area.
	fill_rect(0, SCREEN_H - FONT_H, SCREEN_W, FONT_H, bg_color);
}

void next_line() {
	int y = cursor_pos / TEXT_COLS;
	y += 1;
	if (y >= TEXT_ROWS) {
		scroll();
		y = TEXT_ROWS - 1;
	}
	cursor_pos = y * TEXT_COLS;
}

void rm_char_in_pos(int pos) {
	if (pos < 0) return;
	if (pos >= (TEXT_COLS * TEXT_ROWS)) return;

	int x = pos % TEXT_COLS;
	int y = pos / TEXT_COLS;
	fill_rect(x * FONT_W, y * FONT_H, FONT_W, FONT_H, bg_color);
	cursor_pos = pos;
}

static void move_next_cursor() {
	cursor_pos++;
	if (cursor_pos >= (TEXT_COLS * TEXT_ROWS)) {
		scroll();
		cursor_pos = (TEXT_ROWS - 1) * TEXT_COLS;
	}
}

void kernel_print_char(char c, char toblink) {
	(void)toblink; // blinking not implemented in pixel mode yet

	if (c == '\n') {
		next_line();
		return;
	}

	int cell_x = cursor_pos % TEXT_COLS;
	int cell_y = cursor_pos / TEXT_COLS;

	// Clear cell to avoid "ghosting" when overwriting.
	fill_rect(cell_x * FONT_W, cell_y * FONT_H, FONT_W, FONT_H, bg_color);
	draw_char_at_pixel(cell_x * FONT_W, cell_y * FONT_H, c, 15);

	move_next_cursor();
}

void kernel_print_text(char* text, char toblink) {
	char c = 0;
	(void)toblink;
	while ((c = *text++) != '\0') {
		kernel_print_char(c, toblink);
	}
}