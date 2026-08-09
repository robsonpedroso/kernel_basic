#ifndef _CONFIRM_H
#define _CONFIRM_H

#include "rect.h"

#define CONFIRM_MSG_MAX 64

// Small reusable Yes/No modal, generalizing info.c's About-box-with-OK
// pattern into two buttons. Drawn within the owner's content rect, same as
// lineedit_st's prompt strip -- there is no floating dialog layer in this
// GUI (see menubar.h's note on the same limitation).
typedef struct confirm {
	int active;
	char message[CONFIRM_MSG_MAX];
	int pending_action; // caller-defined id, valid after a 1 (Yes) result
} confirm_st;

void confirm_open(confirm_st *c, const char *message, int pending_action);
void confirm_close(confirm_st *c);
void confirm_draw(confirm_st *c, rect_st content);

// 1 = Yes (c->pending_action is the id passed to confirm_open, already
// closed), -1 = No (already closed), 0 = still open / click missed both
// buttons.
int confirm_mouse_down(confirm_st *c, rect_st content, int lx, int ly);
int confirm_key_down(confirm_st *c, int ascii); // Enter = Yes, Esc = No

#endif
