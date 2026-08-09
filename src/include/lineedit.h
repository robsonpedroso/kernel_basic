#ifndef _LINEEDIT_H
#define _LINEEDIT_H

#include "rect.h"

#define LINEEDIT_MAX 48 // matches FS_NAME_MAX

// Modal one-line prompt strip, drawn across the bottom of a window's
// content area. Append/backspace only -- a filename field doesn't need
// editbuf's cursor-navigation machinery. Shared by file_manager.c (naming
// new files/folders) and text_editor.c (Save As).
typedef struct lineedit {
	char text[LINEEDIT_MAX];
	int  len;
	int  active;
	const char *prompt;
} lineedit_st;

void lineedit_open(lineedit_st *le, const char *prompt);
void lineedit_close(lineedit_st *le);

// 1 = Enter (accepted, text[] is NUL-terminated), -1 = Esc (cancelled,
// already closed), 0 = still editing.
int lineedit_key(lineedit_st *le, int ascii);

void lineedit_draw(lineedit_st *le, rect_st content);

#endif
