#include "../include/video.h"
#include "../include/font8x8_basic.h"
#include "../include/io.h"

// Mode 12h: 640x480, 16 colors, 4 bitplanes (VGA "planar" memory model).
// Each byte at FRAMEBUFFER+offset holds 8 pixels' worth of ONE bit per
// plane; a pixel's actual 4-bit color comes from combining the same bit
// position across all 4 planes. Reading/writing a byte doesn't touch a
// plane directly -- which plane(s) get affected is controlled by the
// Sequencer's Map Mask register and the Graphics Controller's Set/Reset,
// Enable Set/Reset and Bit Mask registers, programmed via port I/O below.
// This is the standard OSDev-wiki "VGA Hardware" technique for planar
// pixel/rect drawing (write mode 0 with Set/Reset) and for VRAM-to-VRAM
// copies (write mode 1, used by scroll()).
#define FRAMEBUFFER 0xA0000

#define VGA_SEQ_INDEX 0x3C4
#define VGA_SEQ_DATA  0x3C5
#define VGA_GC_INDEX  0x3CE
#define VGA_GC_DATA   0x3CF

#define VGA_SEQ_MAP_MASK 0x02

#define VGA_GC_SET_RESET        0x00
#define VGA_GC_ENABLE_SET_RESET 0x01
#define VGA_GC_READ_MAP_SELECT  0x04
#define VGA_GC_MODE             0x05
#define VGA_GC_BIT_MASK         0x08

#define BYTES_PER_ROW (SCREEN_W / 8)

// Simple 8x8 font rendering. Text grid becomes 80 cols x 60 rows.
#define TEXT_COLS (SCREEN_W / FONT_W)
#define TEXT_ROWS (SCREEN_H / FONT_H)

static unsigned char bg_color = 0;
static int cursor_pos = 0; // in "text cells" (not pixels)

static inline void vga_write_gc(unsigned char index, unsigned char value) {
	outb(VGA_GC_INDEX, index);
	outb(VGA_GC_DATA, value);
}

static inline void vga_write_seq(unsigned char index, unsigned char value) {
	outb(VGA_SEQ_INDEX, index);
	outb(VGA_SEQ_DATA, value);
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

int get_cursor_pos() {
	return cursor_pos;
}

void draw_pixel(int x, int y, unsigned char color) {
	if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;

	unsigned int offset = (unsigned int)y * BYTES_PER_ROW + (x / 8);
	unsigned char bit = 0x80 >> (x % 8);
	unsigned char* fb = (unsigned char*)FRAMEBUFFER;

	vga_write_seq(VGA_SEQ_MAP_MASK, 0x0F);          // write to all 4 planes
	vga_write_gc(VGA_GC_SET_RESET, color & 0x0F);    // desired color, per plane
	vga_write_gc(VGA_GC_ENABLE_SET_RESET, 0x0F);     // use Set/Reset for all planes
	vga_write_gc(VGA_GC_BIT_MASK, bit);              // only this pixel's bit changes

	volatile unsigned char latch = fb[offset]; // load the latch with the old byte
	(void)latch;
	fb[offset] = 0xFF; // value is irrelevant: Bit Mask/Set-Reset decide the result
}

// Reads a pixel's 4-bit color back by probing each bitplane in turn via
// the Read Map Select register -- the inverse of draw_pixel's Set/Reset
// trick. Used by the mouse cursor to save/restore what's underneath it.
unsigned char get_pixel(int x, int y) {
	if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return 0;

	unsigned int offset = (unsigned int)y * BYTES_PER_ROW + (x / 8);
	unsigned char bit = 0x80 >> (x % 8);
	unsigned char* fb = (unsigned char*)FRAMEBUFFER;
	unsigned char color = 0;

	for (unsigned char plane = 0; plane < 4; plane++) {
		vga_write_gc(VGA_GC_READ_MAP_SELECT, plane);
		if (fb[offset] & bit) {
			color |= (1 << plane);
		}
	}

	return color;
}

// Fills a horizontal run of pixels on ONE row, byte-aligned, without
// reprogramming the Set/Reset color per pixel (draw_pixel would work but
// is far slower for anything wider than a few pixels).
static void fill_row(unsigned char* fb, unsigned int row_offset, int x, int w) {
	int x_end = x + w; // exclusive
	int first_byte = x / 8;
	int last_byte = (x_end - 1) / 8;

	if (first_byte == last_byte) {
		unsigned char mask = (0xFF >> (x % 8)) & (0xFF << (7 - (x_end - 1) % 8));
		vga_write_gc(VGA_GC_BIT_MASK, mask);
		volatile unsigned char latch = fb[row_offset + first_byte];
		(void)latch;
		fb[row_offset + first_byte] = 0xFF;
		return;
	}

	unsigned char left_mask = 0xFF >> (x % 8);
	vga_write_gc(VGA_GC_BIT_MASK, left_mask);
	volatile unsigned char latch_l = fb[row_offset + first_byte];
	(void)latch_l;
	fb[row_offset + first_byte] = 0xFF;

	if (last_byte - first_byte > 1) {
		vga_write_gc(VGA_GC_BIT_MASK, 0xFF); // full bytes: latch is irrelevant
		for (int b = first_byte + 1; b < last_byte; b++) {
			fb[row_offset + b] = 0xFF;
		}
	}

	unsigned char right_mask = 0xFF << (7 - (x_end - 1) % 8);
	vga_write_gc(VGA_GC_BIT_MASK, right_mask);
	volatile unsigned char latch_r = fb[row_offset + last_byte];
	(void)latch_r;
	fb[row_offset + last_byte] = 0xFF;
}

void fill_rect(int x, int y, int w, int h, unsigned char color) {
	if (w <= 0 || h <= 0) return;
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > SCREEN_W) w = SCREEN_W - x;
	if (y + h > SCREEN_H) h = SCREEN_H - y;
	if (w <= 0 || h <= 0) return;

	unsigned char* fb = (unsigned char*)FRAMEBUFFER;

	vga_write_seq(VGA_SEQ_MAP_MASK, 0x0F);
	vga_write_gc(VGA_GC_SET_RESET, color & 0x0F);
	vga_write_gc(VGA_GC_ENABLE_SET_RESET, 0x0F);

	for (int yy = y; yy < y + h; yy++) {
		fill_row(fb, (unsigned int)yy * BYTES_PER_ROW, x, w);
	}
}

void draw_rect(int x, int y, int w, int h, unsigned char color) {
	if (w <= 0 || h <= 0) return;
	fill_rect(x, y, w, 1, color);
	fill_rect(x, y + h - 1, w, 1, color);
	fill_rect(x, y, 1, h, color);
	fill_rect(x + w - 1, y, 1, h, color);
}

static void draw_char_at_pixel(int x, int y, char c, unsigned char fg) {
	unsigned char uc = (unsigned char)c;
	if (uc >= 128) uc = '?';

	for (int row = 0; row < FONT_H; row++) {
		unsigned char bits = reverse_bits(font8x8_basic[uc][row]);
		for (int col = 0; col < FONT_W; col++) {
			if (bits & (1 << (7 - col))) {
				draw_pixel(x + col, y + row, fg);
			}
		}
	}
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
	// Scroll by one text row (8 pixels) using write mode 1: a read loads
	// the latch with all 4 planes' data at once, and with mode 1 active
	// the following write dumps that latch straight back out (ignoring
	// Set/Reset and Bit Mask entirely) -- an exact, single-pass copy of
	// every plane without touching them individually.
	unsigned char* fb = (unsigned char*)FRAMEBUFFER;

	vga_write_seq(VGA_SEQ_MAP_MASK, 0x0F);
	vga_write_gc(VGA_GC_MODE, 0x01); // write mode 1

	for (int y = FONT_H; y < SCREEN_H; y++) {
		unsigned int dst_row = (unsigned int)(y - FONT_H) * BYTES_PER_ROW;
		unsigned int src_row = (unsigned int)y * BYTES_PER_ROW;
		for (int b = 0; b < BYTES_PER_ROW; b++) {
			volatile unsigned char tmp = fb[src_row + b];
			fb[dst_row + b] = tmp;
		}
	}

	vga_write_gc(VGA_GC_MODE, 0x00); // back to write mode 0 for normal drawing

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
