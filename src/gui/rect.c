#include "../include/rect.h"

int rect_contains_point(rect_st *r, int px, int py) {
	return px >= r->x && px < r->x + r->w &&
	       py >= r->y && py < r->y + r->h;
}
