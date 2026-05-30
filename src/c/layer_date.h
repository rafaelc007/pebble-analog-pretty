#pragma once
#include <pebble.h>

// Creates the static date widget layer ("MON-29") with a small frame so it
// repaints a minimal dirty rect. The date string only changes once per day.
Layer* date_layer_create(GRect bounds, Layer *parent);

// Marks the layer dirty so it re-renders with the current date.
// Call from the main tick handler when DAY_UNIT changes.
void   date_layer_mark_dirty(void);

// Destroys the date layer — call from main_window_unload.
void   date_layer_destroy(void);
