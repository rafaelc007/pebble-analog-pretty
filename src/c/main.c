#include <pebble.h>
#include "watchface.h"
#include "layer_face.h"
#include "layer_hands.h"
#include "layer_weather.h"

// ============================================================================
// PRIVATE STATE
// ============================================================================

static Window *s_main_window;

#define PERSIST_KEY_SHAKE_SECONDS 1

// ============================================================================
// EVENT HANDLERS
// ============================================================================

// Only marks the hands layer dirty — face layer is never touched after init
void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // Exposed externally so layer_hands.c can restore it after seconds hide
  hands_layer_mark_dirty();
}

// ============================================================================
// APPMESSAGE — weather data from pkjs
// ============================================================================

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *temp_tuple  = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
  Tuple *icon_tuple  = dict_find(iterator, MESSAGE_KEY_WEATHER_ICON);
  Tuple *shake_tuple = dict_find(iterator, MESSAGE_KEY_SHAKE_SECONDS_ENABLED);

  if (temp_tuple && icon_tuple) {
    weather_layer_set_data(
      (int)temp_tuple->value->int32,
      (WeatherIconType)icon_tuple->value->int32
    );
  }

  if (shake_tuple) {
    bool enabled = (shake_tuple->value->int32 != 0);
    persist_write_bool(PERSIST_KEY_SHAKE_SECONDS, enabled);
    hands_layer_set_shake_enabled(enabled);
  }
}

static void main_window_load(Window *window) {
  Layer *root   = window_get_root_layer(window);
  GRect  bounds = layer_get_bounds(root);

  // Init shared geometry and font once
  watchface_geometry_init(bounds);

  // Create layers in draw order: face (bottom), weather, then hands on top
  face_layer_create(bounds, root);
  weather_layer_create(bounds, root);
  hands_layer_create(bounds, root);
}

static void main_window_unload(Window *window) {
  face_layer_destroy();
  hands_layer_destroy();
  weather_layer_destroy();
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

  // Apply persisted shake-to-show-seconds preference (default: enabled)
  bool shake_enabled = persist_exists(PERSIST_KEY_SHAKE_SECONDS)
    ? persist_read_bool(PERSIST_KEY_SHAKE_SECONDS)
    : true;
  hands_layer_set_shake_enabled(shake_enabled);

  // AppMessage — receive weather and settings from pkjs
  app_message_register_inbox_received(inbox_received_callback);
  app_message_open(128, 0);
}

static void deinit(void) {
  hands_layer_set_shake_enabled(false);
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}