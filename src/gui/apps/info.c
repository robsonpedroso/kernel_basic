#include "../../include/apps/info.h"
#include "../../include/wm.h"
#include "../../include/video.h"
#include "../../include/gui.h"
#include "../../include/widget.h"
#include "../../include/heap.h"

typedef struct {
	rect_st button_rect_local; // content-relative, for hit-testing in on_mouse_down
} info_state_t;

static void *info_on_init(wm_window_st *win) {
	(void)win;
	return kmalloc(sizeof(info_state_t));
}

static void info_on_show(wm_window_st *win, void *state, rect_st content) {
	(void)win;
	info_state_t *s = (info_state_t *)state;

	fill_rect(content.x, content.y, content.w, content.h, GUI_COLOR_BG);
	draw_text(content.x + 10, content.y + 10, "rSystemOS", GUI_COLOR_BLACK);
	draw_text(content.x + 10, content.y + 26, "v0.5", GUI_COLOR_BLACK);

	int bw = 60, bh = 24;
	s->button_rect_local.x = (content.w - bw) / 2;
	s->button_rect_local.y = content.h - bh - 10;
	s->button_rect_local.w = bw;
	s->button_rect_local.h = bh;

	widget_st btn;
	btn.rect.x = content.x + s->button_rect_local.x;
	btn.rect.y = content.y + s->button_rect_local.y;
	btn.rect.w = bw;
	btn.rect.h = bh;
	btn.label = "OK";
	btn.selected = 0;
	btn.pressed = 0;
	widget_button_draw(&btn);
}

static void info_on_mouse_down(wm_window_st *win, void *state, int lx, int ly, int buttons) {
	(void)buttons;
	info_state_t *s = (info_state_t *)state;
	if (rect_contains_point(&s->button_rect_local, lx, ly)) {
		wm_close_window(win);
	}
}

static void info_on_close(wm_window_st *win, void *state) {
	(void)win;
	kfree(state);
}

const app_st info_app = {
	.name = "Info",
	.icon = ICON_INFO,
	.on_init = info_on_init,
	.on_show = info_on_show,
	.on_key_down = 0,
	.on_mouse_down = info_on_mouse_down,
	.on_mouse_up = 0,
	.on_mouse_move = 0,
	.on_tick = 0,
	.on_close = info_on_close,
};
