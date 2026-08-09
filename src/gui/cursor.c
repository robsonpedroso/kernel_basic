#include "../include/cursor.h"
#include "../include/video.h"

#define CURSOR_SIZE 8

// 0 = transparent (leave background alone), 1 = outline (black), 2 = fill
// (white). A small classic arrow silhouette.
static const unsigned char shape[CURSOR_SIZE][CURSOR_SIZE] = {
	{1, 0, 0, 0, 0, 0, 0, 0},
	{1, 1, 0, 0, 0, 0, 0, 0},
	{1, 2, 1, 0, 0, 0, 0, 0},
	{1, 2, 2, 1, 0, 0, 0, 0},
	{1, 2, 2, 2, 1, 0, 0, 0},
	{1, 2, 2, 2, 2, 1, 0, 0},
	{1, 2, 2, 1, 1, 1, 0, 0},
	{1, 1, 1, 0, 0, 0, 0, 0},
};

static unsigned char backing[CURSOR_SIZE * CURSOR_SIZE];
static int cur_x = -1, cur_y = -1;
static int visible = 0;

static void save_backing(int x, int y) {
	for (int j = 0; j < CURSOR_SIZE; j++) {
		for (int i = 0; i < CURSOR_SIZE; i++) {
			backing[j * CURSOR_SIZE + i] = get_pixel(x + i, y + j);
		}
	}
}

static void restore_backing(int x, int y) {
	for (int j = 0; j < CURSOR_SIZE; j++) {
		for (int i = 0; i < CURSOR_SIZE; i++) {
			draw_pixel(x + i, y + j, backing[j * CURSOR_SIZE + i]);
		}
	}
}

static void draw_shape(int x, int y) {
	for (int j = 0; j < CURSOR_SIZE; j++) {
		for (int i = 0; i < CURSOR_SIZE; i++) {
			unsigned char p = shape[j][i];
			if (p == 1) {
				draw_pixel(x + i, y + j, 0);
			} else if (p == 2) {
				draw_pixel(x + i, y + j, 15);
			}
		}
	}
}

void cursor_init(void) {
	visible = 0;
	cur_x = cur_y = -1;
}

void cursor_show(int x, int y) {
	if (visible) {
		return;
	}
	cur_x = x;
	cur_y = y;
	save_backing(cur_x, cur_y);
	draw_shape(cur_x, cur_y);
	visible = 1;
}

void cursor_hide(void) {
	if (!visible) {
		return;
	}
	restore_backing(cur_x, cur_y);
	visible = 0;
}

void cursor_move_to(int x, int y) {
	if (visible) {
		restore_backing(cur_x, cur_y);
	}
	cur_x = x;
	cur_y = y;
	save_backing(cur_x, cur_y);
	draw_shape(cur_x, cur_y);
	visible = 1;
}
