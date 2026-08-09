#ifndef _WM_FWD_H
#define _WM_FWD_H

// Breaks the circular dependency between app.h (needs a window pointer type
// for its callback signatures) and wm.h (needs app_st to define wm_window_st).
typedef struct wm_window wm_window_st;

#endif
