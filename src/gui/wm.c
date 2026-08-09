#include "../include/wm.h"
#include "../include/video.h"
#include "../include/gui.h"
#include "../include/rect.h"
#include "../include/cursor.h"
#include "../include/mouse.h"
#include "../include/timer.h"
#include "../include/heap.h"
#include "../include/event.h"
#include "../include/taskbar.h"
#include "../include/apps/program_manager.h"

// --- drag/resize state machine -------------------------------------------
typedef enum { WM_IDLE, WM_DRAGGING_TITLE, WM_RESIZING, WM_APP_CAPTURED } wm_mode_t;
typedef enum {
	RESIZE_NONE, RESIZE_N, RESIZE_S, RESIZE_E, RESIZE_W,
	RESIZE_NE, RESIZE_NW, RESIZE_SE, RESIZE_SW,
} resize_edge_t;
#define RESIZE_BORDER 6

typedef enum { HIT_NONE, HIT_SYSMENU, HIT_MINIMIZE, HIT_MAXIMIZE, HIT_TITLE, HIT_RESIZE, HIT_CONTENT } hit_region_t;

// Double-click threshold, in timer ticks (timer.c runs the PIT at 100Hz).
#define DOUBLE_CLICK_TICKS 30

static wm_window_st *g_windows[MAX_WINDOWS];
static int g_window_count = 0;
static wm_window_st *g_focused = 0;

// Start menu: opened from the taskbar's "Iniciar" button. Program Manager
// plus the 5 apps it can launch itself (src/gui/apps/program_manager.c's
// g_launchers[]), reused here instead of a second copy so the two lists
// can't drift apart.
#define START_MENU_COUNT (PM_ICON_COUNT + 1)
static const char *g_start_menu_labels[START_MENU_COUNT] = {
	"Program Manager", "Terminal", "Calculator", "Info", "Arquivos", "Editor",
};
static int g_start_menu_open = 0;

static wm_mode_t g_mode = WM_IDLE;
static wm_window_st *g_drag_win = 0;
static int g_drag_off_x, g_drag_off_y;
static resize_edge_t g_resize_edge = RESIZE_NONE;
static rect_st g_drag_start_rect;
static int g_drag_start_mx, g_drag_start_my;

static int g_dirty = 1; // composite at least once on startup

static wm_window_st *g_last_sysmenu_win = 0;
static unsigned int g_last_sysmenu_tick = 0;

static void wm_composite(void);

void wm_init(void) {
	g_window_count = 0;
	g_focused = 0;
	for (int i = 0; i < MAX_WINDOWS; i++) {
		g_windows[i] = 0;
	}
	g_mode = WM_IDLE;
	g_drag_win = 0;
	g_last_sysmenu_win = 0;
	g_start_menu_open = 0;
}

static rect_st wm_maximized_rect(void) {
	rect_st r;
	r.x = 0;
	r.y = 0;
	r.w = SCREEN_W;
	r.h = SCREEN_H - TASKBAR_HEIGHT;
	return r;
}

static void wm_raise(wm_window_st *w) {
	int idx = -1;
	for (int i = 0; i < g_window_count; i++) {
		if (g_windows[i] == w) {
			idx = i;
			break;
		}
	}
	if (idx < 0) {
		return;
	}
	if (g_focused && g_focused != w && g_focused->app->on_blur) {
		g_focused->app->on_blur(g_focused, g_focused->app_state);
	}
	for (int i = idx; i < g_window_count - 1; i++) {
		g_windows[i] = g_windows[i + 1];
	}
	g_windows[g_window_count - 1] = w;
	g_focused = w;
}

static wm_window_st *wm_topmost_visible(void) {
	for (int i = g_window_count - 1; i >= 0; i--) {
		if (g_windows[i]->state != WIN_STATE_MINIMIZED) {
			return g_windows[i];
		}
	}
	return 0;
}

wm_window_st *wm_create_window(const app_st *app, const char *title, rect_st rect, int min_w, int min_h) {
	if (g_window_count >= MAX_WINDOWS) {
		return 0;
	}

	wm_window_st *w = (wm_window_st *)kmalloc(sizeof(wm_window_st));
	if (!w) {
		return 0;
	}

	w->base.rect = rect;
	w->base.title = title;
	w->restore_rect = rect;
	w->state = WIN_STATE_NORMAL;
	w->restore_state = WIN_STATE_NORMAL;
	w->app = app;
	w->min_w = min_w;
	w->min_h = min_h;
	w->app_state = app->on_init ? app->on_init(w) : 0;

	g_windows[g_window_count++] = w;
	g_focused = w;

	wm_composite();
	return w;
}

void wm_close_window(wm_window_st *win) {
	if (!win) {
		return;
	}
	if (win->app->on_close) {
		win->app->on_close(win, win->app_state);
	}

	int idx = -1;
	for (int i = 0; i < g_window_count; i++) {
		if (g_windows[i] == win) {
			idx = i;
			break;
		}
	}
	if (idx >= 0) {
		for (int i = idx; i < g_window_count - 1; i++) {
			g_windows[i] = g_windows[i + 1];
		}
		g_window_count--;
	}

	if (g_focused == win) {
		g_focused = g_window_count > 0 ? g_windows[g_window_count - 1] : 0;
	}
	if (g_last_sysmenu_win == win) {
		g_last_sysmenu_win = 0;
	}
	if (g_drag_win == win) {
		g_drag_win = 0;
		g_mode = WM_IDLE;
	}

	kfree(win);
	wm_composite();
}

wm_window_st *wm_find_window_by_app(const app_st *app) {
	for (int i = 0; i < g_window_count; i++) {
		if (g_windows[i]->app == app) {
			return g_windows[i];
		}
	}
	return 0;
}

static void wm_minimize_window(wm_window_st *w) {
	if (w->state == WIN_STATE_MINIMIZED) {
		return;
	}
	if (w->state == WIN_STATE_NORMAL) {
		w->restore_rect = w->base.rect;
	}
	w->restore_state = w->state;
	w->state = WIN_STATE_MINIMIZED;

	if (g_focused == w) {
		if (w->app->on_blur) {
			w->app->on_blur(w, w->app_state);
		}
		g_focused = wm_topmost_visible();
	}
}

static void wm_restore_window(wm_window_st *w) {
	if (w->state != WIN_STATE_MINIMIZED) {
		return;
	}
	w->state = w->restore_state;
	w->base.rect = (w->state == WIN_STATE_MAXIMIZED) ? wm_maximized_rect() : w->restore_rect;
	wm_raise(w);
}

// Brings an already-open window to front, restoring it first if it was
// minimized. Used by launchers (e.g. Program Manager) that look a window
// up via wm_find_window_by_app instead of creating a duplicate.
void wm_focus_window(wm_window_st *win) {
	if (!win) return;
	if (win->state == WIN_STATE_MINIMIZED) {
		wm_restore_window(win);   // already calls wm_raise() internally
	} else {
		wm_raise(win);
	}
	g_dirty = 1;
}

void wm_cascade(void) {
	int x = 20, y = 20;
	for (int i = 0; i < g_window_count; i++) {
		wm_window_st *w = g_windows[i];
		if (w->state == WIN_STATE_MINIMIZED) {
			continue;
		}
		w->state = WIN_STATE_NORMAL;
		if (w->base.rect.w < w->min_w) w->base.rect.w = w->min_w;
		if (w->base.rect.h < w->min_h) w->base.rect.h = w->min_h;
		w->base.rect.x = x;
		w->base.rect.y = y;
		x += 24;
		y += 24;
		if (x + w->base.rect.w > SCREEN_W) x = 20;
		if (y + w->base.rect.h > SCREEN_H - TASKBAR_HEIGHT) y = 20;
	}
	g_dirty = 1;
}

void wm_tile(void) {
	int count = 0;
	for (int i = 0; i < g_window_count; i++) {
		if (g_windows[i]->state != WIN_STATE_MINIMIZED) {
			count++;
		}
	}
	if (count == 0) {
		return;
	}

	int cols = 1;
	while (cols * cols < count) {
		cols++;
	}
	int rows = (count + cols - 1) / cols;

	int area_h = SCREEN_H - TASKBAR_HEIGHT;
	int cell_w = SCREEN_W / cols;
	int cell_h = area_h / rows;

	int idx = 0;
	for (int i = 0; i < g_window_count; i++) {
		wm_window_st *w = g_windows[i];
		if (w->state == WIN_STATE_MINIMIZED) {
			continue;
		}
		w->state = WIN_STATE_NORMAL;
		int col = idx % cols;
		int row = idx / cols;
		w->base.rect.x = col * cell_w;
		w->base.rect.y = row * cell_h;
		w->base.rect.w = cell_w;
		w->base.rect.h = cell_h;
		if (w->base.rect.w < w->min_w) w->base.rect.w = w->min_w;
		if (w->base.rect.h < w->min_h) w->base.rect.h = w->min_h;
		idx++;
	}
	g_dirty = 1;
}

static void wm_toggle_maximize(wm_window_st *w) {
	if (w->state == WIN_STATE_MAXIMIZED) {
		w->state = WIN_STATE_NORMAL;
		w->base.rect = w->restore_rect;
	} else {
		w->restore_rect = w->base.rect;
		w->state = WIN_STATE_MAXIMIZED;
		w->base.rect = wm_maximized_rect();
	}
}

static resize_edge_t wm_resize_edge_at(wm_window_st *w, int x, int y) {
	rect_st r = w->base.rect;
	int near_left = (x - r.x) < RESIZE_BORDER;
	int near_right = (r.x + r.w - x) < RESIZE_BORDER;
	int near_top = (y - r.y) < RESIZE_BORDER;
	int near_bottom = (r.y + r.h - y) < RESIZE_BORDER;

	if (near_top && near_left) return RESIZE_NW;
	if (near_top && near_right) return RESIZE_NE;
	if (near_bottom && near_left) return RESIZE_SW;
	if (near_bottom && near_right) return RESIZE_SE;
	if (near_top) return RESIZE_N;
	if (near_bottom) return RESIZE_S;
	if (near_left) return RESIZE_W;
	if (near_right) return RESIZE_E;
	return RESIZE_NONE;
}

static hit_region_t wm_hit_test(wm_window_st *w, int x, int y, resize_edge_t *edge_out) {
	if (w->state == WIN_STATE_MINIMIZED) {
		return HIT_NONE;
	}
	rect_st r = w->base.rect;
	if (!rect_contains_point(&r, x, y)) {
		return HIT_NONE;
	}

	rect_st sysmenu = window_sysmenu_rect(&w->base);
	if (rect_contains_point(&sysmenu, x, y)) return HIT_SYSMENU;
	rect_st minbox = window_minimize_rect(&w->base);
	if (rect_contains_point(&minbox, x, y)) return HIT_MINIMIZE;
	rect_st maxbox = window_maximize_rect(&w->base);
	if (rect_contains_point(&maxbox, x, y)) return HIT_MAXIMIZE;

	int title_bottom = r.y + WINDOW_BORDER + WINDOW_TITLE_HEIGHT;
	if (y < title_bottom) {
		return HIT_TITLE;
	}

	if (w->state == WIN_STATE_NORMAL) {
		resize_edge_t edge = wm_resize_edge_at(w, x, y);
		if (edge != RESIZE_NONE) {
			if (edge_out) *edge_out = edge;
			return HIT_RESIZE;
		}
	}

	return HIT_CONTENT;
}

// Index into g_start_menu_labels[]: 0 = Program Manager (special-cased --
// it isn't in g_launchers[], since Program Manager doesn't list itself in
// its own icon grid), 1..PM_ICON_COUNT = g_launchers[index-1].
static void wm_start_menu_select(int index) {
	if (index == 0) {
		wm_window_st *existing = wm_find_window_by_app(&program_manager_app);
		if (existing) {
			wm_focus_window(existing);
		} else {
			wm_create_window(&program_manager_app, "Program Manager", program_manager_initial_rect(),
			                  PROGRAM_MANAGER_MIN_W, PROGRAM_MANAGER_MIN_H);
		}
		return;
	}
	pm_launch(&g_launchers[index - 1]);
}

void wm_on_mouse_down(int x, int y, int buttons) {
	if (!(buttons & MOUSE_BUTTON_LEFT)) {
		return;
	}

	// 1. Taskbar's Start button: toggles the menu open/closed.
	rect_st start_btn = taskbar_start_button_rect();
	if (rect_contains_point(&start_btn, x, y)) {
		g_start_menu_open = !g_start_menu_open;
		g_dirty = 1;
		return;
	}

	// 2. Start menu, if open: select an item, or dismiss it. A dismissing
	// click (outside the menu and outside the Start button, already ruled
	// out above) is NOT consumed here -- it falls through to the checks
	// below so the same click still reaches whatever's underneath, matching
	// real Start-menu behavior.
	if (g_start_menu_open) {
		int item = taskbar_hit_start_item(g_start_menu_labels, START_MENU_COUNT, x, y);
		if (item >= 0) {
			wm_start_menu_select(item);
			g_start_menu_open = 0;
			g_dirty = 1;
			return;
		}
		g_start_menu_open = 0;
		g_dirty = 1;
	}

	// 3. Taskbar window buttons -- one per open window (minimized or not);
	// clicking always focuses/restores (never toggles to minimize).
	int btn = taskbar_hit_button(g_window_count, x, y);
	if (btn >= 0) {
		wm_focus_window(g_windows[btn]);
		return;
	}

	// 4. Windows, topmost first.
	for (int i = g_window_count - 1; i >= 0; i--) {
		wm_window_st *w = g_windows[i];
		resize_edge_t edge = RESIZE_NONE;
		hit_region_t hit = wm_hit_test(w, x, y, &edge);
		if (hit == HIT_NONE) {
			continue;
		}

		wm_raise(w);

		switch (hit) {
			case HIT_SYSMENU: {
				unsigned int now = timer_get_ticks();
				if (g_last_sysmenu_win == w && (now - g_last_sysmenu_tick) < DOUBLE_CLICK_TICKS) {
					g_last_sysmenu_win = 0;
					wm_close_window(w);
					return;
				}
				g_last_sysmenu_win = w;
				g_last_sysmenu_tick = now;
				break;
			}
			case HIT_MINIMIZE:
				wm_minimize_window(w);
				break;
			case HIT_MAXIMIZE:
				wm_toggle_maximize(w);
				break;
			case HIT_TITLE:
				g_mode = WM_DRAGGING_TITLE;
				g_drag_win = w;
				g_drag_off_x = x - w->base.rect.x;
				g_drag_off_y = y - w->base.rect.y;
				break;
			case HIT_RESIZE:
				g_mode = WM_RESIZING;
				g_drag_win = w;
				g_resize_edge = edge;
				g_drag_start_rect = w->base.rect;
				g_drag_start_mx = x;
				g_drag_start_my = y;
				break;
			case HIT_CONTENT: {
				rect_st content = window_content_rect(&w->base);
				int lx = x - content.x, ly = y - content.y;
				if (w->app->on_mouse_down) {
					w->app->on_mouse_down(w, w->app_state, lx, ly, buttons);
				}
				g_mode = WM_APP_CAPTURED;
				g_drag_win = w;
				break;
			}
			default:
				break;
		}
		g_dirty = 1;
		return;
	}
}

void wm_on_mouse_move(int x, int y) {
	switch (g_mode) {
		case WM_DRAGGING_TITLE: {
			int nx = x - g_drag_off_x;
			int ny = y - g_drag_off_y;
			if (nx < 0) nx = 0;
			if (ny < 0) ny = 0;
			if (nx > SCREEN_W - 20) nx = SCREEN_W - 20;
			if (ny > SCREEN_H - TASKBAR_HEIGHT - 20) ny = SCREEN_H - TASKBAR_HEIGHT - 20;
			g_drag_win->base.rect.x = nx;
			g_drag_win->base.rect.y = ny;
			g_dirty = 1;
			break;
		}
		case WM_RESIZING: {
			int dx = x - g_drag_start_mx;
			int dy = y - g_drag_start_my;
			rect_st r = g_drag_start_rect;

			switch (g_resize_edge) {
				case RESIZE_E:  r.w += dx; break;
				case RESIZE_S:  r.h += dy; break;
				case RESIZE_W:  r.x += dx; r.w -= dx; break;
				case RESIZE_N:  r.y += dy; r.h -= dy; break;
				case RESIZE_SE: r.w += dx; r.h += dy; break;
				case RESIZE_SW: r.x += dx; r.w -= dx; r.h += dy; break;
				case RESIZE_NE: r.y += dy; r.h -= dy; r.w += dx; break;
				case RESIZE_NW: r.x += dx; r.w -= dx; r.y += dy; r.h -= dy; break;
				default: break;
			}

			if (r.w < g_drag_win->min_w) r.w = g_drag_win->min_w;
			if (r.h < g_drag_win->min_h) r.h = g_drag_win->min_h;
			if (r.x + r.w > SCREEN_W) r.w = SCREEN_W - r.x;
			if (r.y + r.h > SCREEN_H - TASKBAR_HEIGHT) r.h = SCREEN_H - TASKBAR_HEIGHT - r.y;

			g_drag_win->base.rect = r;
			g_dirty = 1;
			break;
		}
		case WM_APP_CAPTURED: {
			rect_st content = window_content_rect(&g_drag_win->base);
			int lx = x - content.x, ly = y - content.y;
			if (g_drag_win->app->on_mouse_move) {
				g_drag_win->app->on_mouse_move(g_drag_win, g_drag_win->app_state, lx, ly);
			}
			g_dirty = 1;
			break;
		}
		default:
			break;
	}

	// Lightweight cursor tracking, independent of g_dirty: this only
	// touches the small area under the cursor sprite (see cursor.c), not a
	// full desktop repaint, so it's safe/cheap to do on every move even
	// while idle. Full repaints (window content, chrome) still wait for
	// the next tick via g_dirty.
	cursor_move_to(x, y);
}

void wm_on_mouse_up(int x, int y, int buttons) {
	if (g_mode == WM_APP_CAPTURED && g_drag_win) {
		rect_st content = window_content_rect(&g_drag_win->base);
		int lx = x - content.x, ly = y - content.y;
		if (g_drag_win->app->on_mouse_up) {
			g_drag_win->app->on_mouse_up(g_drag_win, g_drag_win->app_state, lx, ly, buttons);
		}
	}
	g_mode = WM_IDLE;
	g_drag_win = 0;
	g_dirty = 1;
}

void wm_on_key_down(int ascii, int mods) {
	if (g_focused && g_focused->app->on_key_down) {
		g_focused->app->on_key_down(g_focused, g_focused->app_state, ascii, mods);
		g_dirty = 1;
	}
}

void wm_on_tick(void) {
	for (int i = 0; i < g_window_count; i++) {
		wm_window_st *w = g_windows[i];
		if (w->state == WIN_STATE_MINIMIZED) {
			continue;
		}
		if (w->app->on_tick && w->app->on_tick(w, w->app_state)) {
			g_dirty = 1;
		}
	}
	if (!g_dirty) {
		return;
	}
	g_dirty = 0;
	wm_composite();
}

static void wm_composite(void) {
	cursor_hide();

	fill_rect(0, 0, SCREEN_W, SCREEN_H, GUI_COLOR_DESKTOP);

	for (int i = 0; i < g_window_count; i++) {
		wm_window_st *w = g_windows[i];
		if (w->state == WIN_STATE_MINIMIZED) {
			continue;
		}
		window_draw(&w->base, w == g_focused);
		rect_st content = window_content_rect(&w->base);
		if (w->app->on_show) {
			w->app->on_show(w, w->app_state, content);
		}
	}

	// Taskbar (and its Start menu, if open) is drawn last so it floats on
	// top of every window, including one whose rect happens to butt right
	// up against the reserved bottom strip.
	taskbar_button_t buttons[MAX_WINDOWS];
	for (int i = 0; i < g_window_count; i++) {
		wm_window_st *w = g_windows[i];
		buttons[i].title = w->base.title;
		buttons[i].focused = (w == g_focused);
		buttons[i].minimized = (w->state == WIN_STATE_MINIMIZED);
	}
	taskbar_draw_bar(g_start_menu_open, buttons, g_window_count);
	if (g_start_menu_open) {
		taskbar_draw_start_menu(g_start_menu_labels, START_MENU_COUNT, -1);
	}

	cursor_show(mouse_get_x(), mouse_get_y());
}
