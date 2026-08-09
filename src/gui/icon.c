#include "../include/icon.h"
#include "../include/icon_data.h"
#include "../include/video.h"
#include "../include/gui.h"
#include "../include/string.h"
#include "../include/widget.h"

static const unsigned char *icon_bitmap(icon_id_t icon) {
	switch (icon) {
		case ICON_TERMINAL:      return icon_terminal;
		case ICON_CALCULATOR:    return icon_calculator;
		case ICON_INFO:          return icon_info;
		case ICON_FILE_MANAGER:  return icon_arquivos;
		case ICON_TEXT_EDITOR:   return icon_editor;
		default:                 return 0;
	}
}

static void icon_draw_placeholder(rect_st box) {
	fill_rect(box.x, box.y, box.w, box.h, GUI_COLOR_BG);
	draw_bevel(box.x, box.y, box.w, box.h, 0);
}

// Mirrors draw_char_at_pixel's row/col plot loop in video.c, just one byte
// per pixel (16-color index) instead of one bit per pixel (monochrome).
static void icon_draw_bitmap(rect_st box, const unsigned char *bitmap) {
	for (int row = 0; row < ICON_BITMAP_SIZE; row++) {
		for (int col = 0; col < ICON_BITMAP_SIZE; col++) {
			unsigned char idx = bitmap[row * ICON_BITMAP_SIZE + col];
			if (idx != ICON_TRANSPARENT) {
				draw_pixel(box.x + col, box.y + row, idx);
			}
		}
	}
}

void icon_draw(rect_st box, const char *label, icon_id_t icon) {
	const unsigned char *bitmap = icon_bitmap(icon);
	if (bitmap) {
		icon_draw_bitmap(box, bitmap);
	} else {
		icon_draw_placeholder(box);
	}

	int text_len = strlen((char *)label);
	int text_w = text_len * FONT_W;
	int tx = box.x + (box.w - text_w) / 2;
	int ty = box.y + box.h + 2;
	draw_text(tx, ty, (char *)label, GUI_COLOR_LIGHT);
}
