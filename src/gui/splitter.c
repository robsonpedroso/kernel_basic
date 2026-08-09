#include "../include/splitter.h"
#include "../include/video.h"
#include "../include/gui.h"

rect_st splitter_bar_rect(rect_st pane_area, int split_x) {
	rect_st bar = { pane_area.x + split_x, pane_area.y, SPLITTER_WIDTH, pane_area.h };
	return bar;
}

void splitter_draw(rect_st bar) {
	fill_rect(bar.x, bar.y, bar.w, bar.h, GUI_COLOR_BG);
	fill_rect(bar.x, bar.y, 1, bar.h, GUI_COLOR_SHADOW);
	fill_rect(bar.x + bar.w - 1, bar.y, 1, bar.h, GUI_COLOR_LIGHT);
}

int splitter_hit_test(rect_st bar, int x, int y) {
	return rect_contains_point(&bar, x, y);
}
