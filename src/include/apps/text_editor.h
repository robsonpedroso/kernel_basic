#ifndef _APPS_TEXT_EDITOR_H
#define _APPS_TEXT_EDITOR_H

#include "../app.h"

#define TEXT_EDITOR_MIN_W 440
#define TEXT_EDITOR_MIN_H 224

extern const app_st text_editor_app;

// Call immediately before wm_create_window() to aim the next instance at an
// existing file (file_id >= 0) or at a folder to save a new document into.
// on_init consumes it and resets the pending values, so a plain launch
// (e.g. from Program Manager, which never calls this) opens an untitled
// document at the root. Safe without locking: wm_create_window() calls
// on_init synchronously, and this kernel is single-threaded/cooperative.
void text_editor_set_target(int file_id, int parent);

#endif
