#include "../../include/apps/program_manager.h"
#include "../../include/apps/terminal.h"
#include "../../include/apps/calculator.h"
#include "../../include/apps/info.h"
#include "../../include/apps/file_manager.h"
#include "../../include/apps/text_editor.h"
#include "../../include/wm.h"
#include "../../include/icon.h"
#include "../../include/rect.h"
#include "../../include/video.h"
#include "../../include/gui.h"
#include "../../include/heap.h"
#include "../../include/timer.h"

#define PM_ICON_SIZE 32
// Wide enough that the longest label ("Calculator", 10 chars) doesn't
// overlap its neighbors once icon_draw() centers it under the (much
// narrower) icon box.
#define PM_ICON_MARGIN 48
#define PM_ICON_ROW_STEP 54 // icon (32) + label (8) + gap (14)
// Own, independent double-click threshold -- wm.c's DOUBLE_CLICK_TICKS is
// static, not reachable from here; same 300ms-@-100Hz rationale, duplicated
// on purpose.
#define PM_DOUBLE_CLICK_TICKS 30

// Rects chosen to stay fully on-screen (SCREEN_W=640, SCREEN_H=480) even
// though wm.c doesn't clamp a window's *initial* creation rect.
const pm_launcher_entry_t g_launchers[PM_ICON_COUNT] = {
	{ &terminal_app,     "Terminal",   "Terminal",     {  40,  40, 340, 220 }, TERMINAL_MIN_W,     TERMINAL_MIN_H,     1 },
	{ &calculator_app,   "Calculator", "Calculator",   { 400,  40, 200, 220 }, CALCULATOR_MIN_W,   CALCULATOR_MIN_H,   1 },
	{ &info_app,         "Info",       "Info",         { 150, 280, 260, 180 }, 150, 100,                               1 },
	{ &file_manager_app, "Arquivos",   "Arquivos",     {  60,  60, 400, 300 }, FILE_MANAGER_MIN_W, FILE_MANAGER_MIN_H, 1 },
	{ &text_editor_app,  "Editor",     "Text Editor",  { 120,  60, 460, 300 }, TEXT_EDITOR_MIN_W,  TEXT_EDITOR_MIN_H,  0 },
};

typedef struct {
	rect_st icon_rects[PM_ICON_COUNT];   // content-relative, refreshed every on_show
	int last_click_index;                 // -1 = none
	unsigned int last_click_tick;
} program_manager_state_t;

void pm_launch(const pm_launcher_entry_t *entry) {
	if (entry->singleton) {
		wm_window_st *existing = wm_find_window_by_app(entry->app);
		if (existing) {
			wm_focus_window(existing);
			return;
		}
	}
	wm_create_window(entry->app, entry->title, entry->initial_rect, entry->min_w, entry->min_h);
}

static void *pm_on_init(wm_window_st *win) {
	(void)win;
	program_manager_state_t *s = (program_manager_state_t *)kmalloc(sizeof(program_manager_state_t));
	s->last_click_index = -1;
	s->last_click_tick = 0;
	return s;
}

static void pm_on_show(wm_window_st *win, void *state, rect_st content) {
	(void)win;
	program_manager_state_t *s = (program_manager_state_t *)state;
	fill_rect(content.x, content.y, content.w, content.h, GUI_COLOR_BG);

	int x = 12, y = 12;
	for (int i = 0; i < PM_ICON_COUNT; i++) {
		rect_st box = { x, y, PM_ICON_SIZE, PM_ICON_SIZE };
		s->icon_rects[i] = box;

		rect_st abs = { content.x + box.x, content.y + box.y, box.w, box.h };
		icon_draw(abs, g_launchers[i].label, g_launchers[i].app->icon);

		x += PM_ICON_SIZE + PM_ICON_MARGIN;
		if (x + PM_ICON_SIZE > content.w) {
			x = 12;
			y += PM_ICON_ROW_STEP;
		}
	}
}

static void pm_on_mouse_down(wm_window_st *win, void *state, int lx, int ly, int buttons) {
	(void)win;
	(void)buttons;
	program_manager_state_t *s = (program_manager_state_t *)state;
	for (int i = 0; i < PM_ICON_COUNT; i++) {
		if (!rect_contains_point(&s->icon_rects[i], lx, ly)) continue;

		unsigned int now = timer_get_ticks();
		if (s->last_click_index == i && (now - s->last_click_tick) < PM_DOUBLE_CLICK_TICKS) {
			s->last_click_index = -1;
			pm_launch(&g_launchers[i]);
		} else {
			s->last_click_index = i;
			s->last_click_tick = now;
		}
		return;
	}
}

static void pm_on_close(wm_window_st *win, void *state) {
	(void)win;
	kfree(state);
}

rect_st program_manager_initial_rect(void) {
	rect_st r = { 120, 90, 400, 200 };
	return r;
}

const app_st program_manager_app = {
	.name = "Program Manager",
	.on_init = pm_on_init,
	.on_show = pm_on_show,
	.on_key_down = 0,
	.on_mouse_down = pm_on_mouse_down,
	.on_mouse_up = 0,
	.on_mouse_move = 0,
	.on_tick = 0,
	.on_close = pm_on_close,
};
