#ifndef _CURSOR_H
#define _CURSOR_H

// Draws a small arrow sprite at the given screen position, saving whatever
// was underneath it first so it can be restored later. Call cursor_hide()
// before any other drawing touches the screen, and cursor_show()/
// cursor_move_to() afterward -- there's no shadow framebuffer, so the
// cursor is only ever "correct" relative to whatever was drawn last.
void cursor_init(void);
void cursor_show(int x, int y);
void cursor_hide(void);
void cursor_move_to(int x, int y);

#endif
