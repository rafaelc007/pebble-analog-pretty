#include <pebble.h>
#include "watchface.h"
#include "layer_face.h"
#include "layer_hands.h"

// ============================================================================
// PRIVATE STATE
// ============================================================================

static Window *s_main_window;

// ============================================================================
// EVENT HANDLERS
// ============================================================================

// Only marks the hands layer dirty — face layer is never touched after init
void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // Exposed externally so layer_hands.c can restore it after seconds hide
  hands_layer_mark_dirty();
}

static void main_window_load(Window *window) {
  Layer *root   = window_get_root_layer(window);
  GRect  bounds = layer_get_bounds(root);

  // Init shared geometry and font once
  watchface_geometry_init(bounds);

  // Create layers in draw order: face first (bottom), hands on top
  face_layer_create(bounds, root);
  hands_layer_create(bounds, root);
}

static void main_window_unload(Window *window) {
  face_layer_destroy();
  hands_layer_destroy();
}

// ============================================================================
// APP LIFECYCLE
// ============================================================================

static void init(void) {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load   = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  accel_tap_service_subscribe(hands_layer_handle_tap);
}

static void deinit(void) {
  accel_tap_service_unsubscribe();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}