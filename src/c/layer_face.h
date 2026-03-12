#pragma once
#include <pebble.h>

// Creates the static face layer (clock ring + markers + hour numbers)
// Parent layer is the window root layer
// This layer is drawn ONCE and never marked dirty again
Layer* face_layer_create(GRect bounds, Layer *parent);

// Destroys the face layer — call from main_window_unload
void face_layer_destroy(void);