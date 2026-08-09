#ifndef _WM_H
#define _WM_H

#include "rect.h"
#include "window.h"
#include "wm_fwd.h"
#include "app.h"

typedef enum {
	WIN_STATE_NORMAL,
	WIN_STATE_MINIMIZED,
	WIN_STATE_MAXIMIZED,
} window_state_t;

struct wm_window {
	window_st base;               // rect + title; window.c draws this part
	rect_st restore_rect;          // rect to return to from minimize/maximize
	window_state_t state;
	window_state_t restore_state;  // state to return to when un-minimized
	const app_st *app;
	void *app_state;                // opaque, returned by app->on_init
	int min_w, min_h;               // resize floor
};

#define MAX_WINDOWS 8

void wm_init(void);

// rect is the initial NORMAL-state geometry. Calls app->on_init, adds the
// window on top of the z-order, focuses it, and composites immediately.
wm_window_st *wm_create_window(const app_st *app, const char *title, rect_st rect, int min_w, int min_h);

void wm_close_window(wm_window_st *win);

// Finds an already-open window for a given app (singleton lookup), or NULL.
wm_window_st *wm_find_window_by_app(const app_st *app);

// Brings an already-open window to front, restoring it first if minimized.
void wm_focus_window(wm_window_st *win);

// Repositions every open, non-minimized window (desktop-wide, not scoped to
// any one app -- this GUI has no MDI/child-window concept, so there is no
// narrower "just this app's documents" grouping to cascade/tile within).
void wm_cascade(void);
void wm_tile(void);

// Wired directly to kernel.c's event loop.
void wm_on_mouse_move(int x, int y);
void wm_on_mouse_down(int x, int y, int buttons);
void wm_on_mouse_up(int x, int y, int buttons);
void wm_on_key_down(int ascii, int mods);
void wm_on_tick(void);

#endif
