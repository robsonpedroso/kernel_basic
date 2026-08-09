#ifndef _RECT_H
#define _RECT_H

typedef struct rect {
	int x, y, w, h;
} rect_st;

int rect_contains_point(rect_st *r, int px, int py);

#endif
