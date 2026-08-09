#include "../include/scrollbar.h"
#include "../include/video.h"
#include "../include/gui.h"
#include "../include/widget.h"

static rect_st scrollbar_thumb_rect(scrollbar_st *sb) {
	int track_h = sb->rect.h;
	int total = sb->total > 0 ? sb->total : 1;
	int thumb_h = (sb->visible * track_h) / total;
	if (thumb_h < 8) {
		thumb_h = 8;
	}
	if (thumb_h > track_h) {
		thumb_h = track_h;
	}
	int max_pos = sb->total - sb->visible;
	int thumb_y = sb->rect.y;
	if (max_pos > 0) {
		thumb_y += ((track_h - thumb_h) * sb->pos) / max_pos;
	}
	rect_st r = { sb->rect.x, thumb_y, sb->rect.w, thumb_h };
	return r;
}

void scrollbar_draw(scrollbar_st *sb) {
	fill_rect(sb->rect.x, sb->rect.y, sb->rect.w, sb->rect.h, GUI_COLOR_BG);
	draw_rect(sb->rect.x, sb->rect.y, sb->rect.w, sb->rect.h, GUI_COLOR_SHADOW);
	if (sb->total <= sb->visible) {
		return; // nothing to scroll, bare track only
	}

	rect_st thumb = scrollbar_thumb_rect(sb);
	fill_rect(thumb.x, thumb.y, thumb.w, thumb.h, GUI_COLOR_BG);
	draw_bevel(thumb.x, thumb.y, thumb.w, thumb.h, 0);
}

int scrollbar_hit_test(scrollbar_st *sb, int x, int y) {
	if (!rect_contains_point(&sb->rect, x, y)) {
		return -1;
	}
	if (sb->total <= sb->visible) {
		return 0; // whole track, nothing to drag
	}
	rect_st thumb = scrollbar_thumb_rect(sb);
	if (rect_contains_point(&thumb, x, y)) {
		return 1;
	}
	return 0;
}

void scrollbar_drag_to(scrollbar_st *sb, int y) {
	int max_pos = sb->total - sb->visible;
	if (max_pos <= 0) {
		sb->pos = 0;
		return;
	}
	rect_st thumb = scrollbar_thumb_rect(sb);
	int usable = sb->rect.h - thumb.h;
	if (usable <= 0) {
		sb->pos = 0;
		return;
	}
	int rel = y - sb->rect.y - thumb.h / 2;
	int pos = (rel * max_pos) / usable;
	if (pos < 0) {
		pos = 0;
	}
	if (pos > max_pos) {
		pos = max_pos;
	}
	sb->pos = pos;
}
