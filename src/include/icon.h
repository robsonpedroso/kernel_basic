#ifndef _ICON_H
#define _ICON_H

#include "rect.h"

// Identifies which 32x32 bitmap (if any) an app_st should draw instead of
// the bevel-box placeholder -- see icon_data.h (private to icon.c) for the
// actual pixel data, converted once from icones/icones-la-capitaine-no-linux-1.webp.
// ICON_NONE (0) is the default for every app_st that doesn't set .icon,
// so untouched apps keep today's placeholder with zero changes.
typedef enum {
	ICON_NONE = 0,
	ICON_TERMINAL,
	ICON_CALCULATOR,
	ICON_INFO,
	ICON_FILE_MANAGER,
	ICON_TEXT_EDITOR,
	ICON_COUNT
} icon_id_t;

#define ICON_BITMAP_SIZE 32
#define ICON_TRANSPARENT 0xFF

// Draws `icon` (32x32, one byte per pixel, VGA palette index 0-15 or
// ICON_TRANSPARENT) at box.x/box.y with a centered label below it -- no
// bevel box behind a real bitmap (matches authentic Win3.1 icon look).
// Falls back to the original bevel-box + label placeholder when icon is
// ICON_NONE or has no bitmap.
void icon_draw(rect_st box, const char *label, icon_id_t icon);

#endif
