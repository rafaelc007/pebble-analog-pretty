#pragma once
#include <pebble.h>

// Creates the dynamic hands layer (hour + minute hands + date widget)
// Sits on top of the face layer — redrawn every minute tick
Layer* hands_layer_create(GRect bounds, Layer *parent);

// Call from tick_handler to refresh hands and date
void hands_layer_mark_dirty(void);

// Destroys the hands layer — call from main_window_unload
void hands_layer_destroy(void);

// Shake to show feature
void   hands_layer_handle_tap(AccelAxisType axis, int32_t direction);

// Enable / disable the shake-to-show-seconds feature at runtime.
// When disabled, the accelerometer subscription is dropped to save power.
void   hands_layer_set_shake_enabled(bool enabled);

// Set how long (in milliseconds) the seconds hand stays visible after a shake.
// Clamped to [SECONDS_DISPLAY_DURATION_MIN, SECONDS_DISPLAY_DURATION_MAX].
void   hands_layer_set_seconds_duration(uint32_t duration_ms);