#pragma once
#include <pebble.h>

// Creates the dynamic hands layer (hour + minute hands + date widget)
// Sits on top of the face layer — redrawn every minute tick
Layer* hands_layer_create(GRect bounds, Layer *parent);

// Call from tick_handler to refresh hands and date
void hands_layer_mark_dirty(void);

// Destroys the hands layer — call from main_window_unload
void hands_layer_destroy(void);