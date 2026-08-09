#ifndef _TASKBAR_H
#define _TASKBAR_H

#include "rect.h"

// Reserved strip at the bottom of the screen -- wm.c subtracts this from
// every "usable desktop area" computation (maximize, cascade, tile, the
// drag/resize clamp), same role the old minimized-icon tray height used to
// play. Sized like a title bar (WINDOW_TITLE_HEIGHT=20) plus a small
// margin, not the old 56px: that value existed only to fit a 32px icon +
// label + gap, which no longer applies to a row of text buttons.
#define TASKBAR_HEIGHT 24

// Row height for each entry in the open Start menu dropdown.
#define TASKBAR_ITEM_H 16

// One taskbar button = one open window (wm.c owns the window list; this
// widget just draws/hit-tests, same "dumb widget" role window.c/menubar.c
// already play). `minimized` only changes the label's color (dimmer),
// `focused` draws the button pressed/highlighted.
typedef struct {
	const char *title;
	int focused;
	int minimized;
} taskbar_button_t;

rect_st taskbar_bar_rect(void);
rect_st taskbar_start_button_rect(void);

// Draws the whole bar: background, the Start button (pressed-looking if
// start_open), and one button per entry in `buttons`. Does not draw the
// Start menu dropdown -- callers that want it open call
// taskbar_draw_start_menu separately, last, so it paints on top of
// everything else (including windows), the same way menubar.c splits
// _draw_bar/_draw_dropdown for callers with content below the bar.
void taskbar_draw_bar(int start_open, const taskbar_button_t *buttons, int button_count);

// Draws the Start menu, opened upward from the Start button (the bar lives
// at the bottom of the screen, so a downward dropdown would run off
// screen). Visual style matches menubar_draw_dropdown: filled box, bevel
// border, hovered row highlighted in GUI_COLOR_TITLE.
void taskbar_draw_start_menu(const char *const *labels, int label_count, int hover_index);

// -1 if (x, y) isn't over any window button, else its index into the
// `buttons` array last passed to taskbar_draw_bar (button geometry is
// recomputed from button_count alone, same pattern as
// menubar_dropdown_row_at recomputing from mb->open_index).
int taskbar_hit_button(int button_count, int x, int y);

// -1 if (x, y) isn't over any Start menu row, else its index into `labels`.
// Takes the same `labels`/`label_count` the menu was last drawn with --
// needed to recompute the identical dropdown rect (its width depends on the
// longest label).
int taskbar_hit_start_item(const char *const *labels, int label_count, int x, int y);

#endif
