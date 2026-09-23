#ifndef _APPS_PROGRAM_MANAGER_H
#define _APPS_PROGRAM_MANAGER_H

#include "../app.h"
#include "../rect.h"

#define PROGRAM_MANAGER_MIN_W 360
#define PROGRAM_MANAGER_MIN_H 180

#define PM_ICON_COUNT 5

// One entry per app launchable from the Program Manager icon grid. Exported
// (not static to program_manager.c) so the taskbar's Start menu (wm.c) can
// walk the exact same table instead of keeping a second copy that could
// drift out of sync.
typedef struct {
	const app_st *app;
	const char *label;
	const char *title;
	rect_st initial_rect;
	int min_w, min_h;
	int singleton; // 0 = always open a fresh instance (Text Editor)
} pm_launcher_entry_t;

extern const pm_launcher_entry_t g_launchers[PM_ICON_COUNT];

// Focuses entry->app's existing window if it's a singleton already open,
// else creates a new one. Shared by the Program Manager icon grid and the
// taskbar's Start menu.
void pm_launch(const pm_launcher_entry_t *entry);

// Program Manager's own initial geometry -- shared by kernel.c (boot) and
// the taskbar's Start menu (reopening it after it's been closed), so the
// rect literal lives in exactly one place.
rect_st program_manager_initial_rect(void);

extern const app_st program_manager_app;

#endif
