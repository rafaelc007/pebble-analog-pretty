#include <pebble.h>
#include "watchface.h"
#include "layer_face.h"
#include "layer_hands.h"
#include "layer_weather.h"
#include "layer_date.h"

// ============================================================================
// PRIVATE STATE
// ============================================================================

static Window *s_main_window;

#define PERSIST_KEY_SHAKE_SECONDS 1
#define PERSIST_KEY_SECONDS_DURATION 2
#define PERSIST_KEY_TEMPERATURE_UNIT 3
#define PERSIST_KEY_THEME_COLOR 4

#define DEFAULT_THEME_COLOR_ARGB 0b11001111  // GColorCyan

// ============================================================================
// EVENT HANDLERS
// ============================================================================

// Ask pkjs to refetch and resend weather. Sends a 1-key AppMessage with the
// pre-existing `dummy` key, which the pkjs `appmessage` handler treats as a
// "please refresh" signal. Skips when the phone is disconnected to avoid
// pointless outbox attempts and JS runtime wake-ups.
static void request_weather_refresh(void) {
  if (!connection_service_peek_pebble_app_connection()) return;
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  dict_write_uint8(iter, MESSAGE_KEY_dummy, 1);
  app_message_outbox_send();
}

// Marks the hands layer dirty every tick. Also bubbles HOUR_UNIT / DAY_UNIT
// changes to the relevant static layers, and triggers an hourly weather
// refresh — all piggybacking on the existing tick, no extra timers.
void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // Exposed externally so layer_hands.c can restore it after seconds hide
  hands_layer_mark_dirty();

  if (units_changed & HOUR_UNIT) {
    face_layer_update_hour(tick_time->tm_hour);
    request_weather_refresh();
  }
  if (units_changed & DAY_UNIT) {
    date_layer_mark_dirty();
  }
}

// ============================================================================
// CONNECTION SERVICE
// ============================================================================

static void connection_handler(bool connected) {
  weather_layer_set_connected(connected);
}

// ============================================================================
// APPMESSAGE — weather data from pkjs
// ============================================================================

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *temp_tuple  = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
  Tuple *icon_tuple  = dict_find(iterator, MESSAGE_KEY_WEATHER_ICON);
  Tuple *theme_tuple = dict_find(iterator, MESSAGE_KEY_THEME_COLOR);
  Tuple *shake_tuple = dict_find(iterator, MESSAGE_KEY_SHAKE_SECONDS_ENABLED);
  Tuple *dur_tuple   = dict_find(iterator, MESSAGE_KEY_SECONDS_DURATION);
  Tuple *unit_tuple  = dict_find(iterator, MESSAGE_KEY_TEMPERATURE_UNIT);

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

  if (dur_tuple) {
    // Sent from pkjs in seconds; convert to ms for the watch-side timer
    int32_t seconds = dur_tuple->value->int32;
    uint32_t duration_ms = (uint32_t)seconds * 1000;
    persist_write_int(PERSIST_KEY_SECONDS_DURATION, (int32_t)duration_ms);
    hands_layer_set_seconds_duration(duration_ms);
  }

  if (unit_tuple) {
    bool fahrenheit = (unit_tuple->value->int32 != 0);
    persist_write_bool(PERSIST_KEY_TEMPERATURE_UNIT, fahrenheit);
    weather_layer_set_fahrenheit(fahrenheit);
  }

  if (theme_tuple) {
    uint8_t argb = (uint8_t)theme_tuple->value->int32;
    persist_write_int(PERSIST_KEY_THEME_COLOR, (int32_t)argb);
    watchface_set_theme_color(argb);
  }
}

static void main_window_load(Window *window) {
  Layer *root   = window_get_root_layer(window);
  GRect  bounds = layer_get_bounds(root);

  // Init shared geometry and font once
  watchface_geometry_init(bounds);

  // Create layers in draw order: face (bottom), weather, date, then hands on top
  face_layer_create(bounds, root);
  weather_layer_create(bounds, root);
  date_layer_create(bounds, root);
  hands_layer_create(bounds, root);

  // Initial hour highlight — done here since the hands layer no longer
  // updates it on every minute tick.
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  face_layer_update_hour(t->tm_hour);
}

static void main_window_unload(Window *window) {
  face_layer_destroy();
  hands_layer_destroy();
  weather_layer_destroy();
  date_layer_destroy();
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

  // Apply persisted theme color (default: cyan). Must happen before the
  // window loads so the first draw uses the right color.
  uint8_t theme_argb = persist_exists(PERSIST_KEY_THEME_COLOR)
    ? (uint8_t)persist_read_int(PERSIST_KEY_THEME_COLOR)
    : DEFAULT_THEME_COLOR_ARGB;
  watchface_set_theme_color(theme_argb);

  window_stack_push(s_main_window, true);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Apply persisted shake-to-show-seconds preference (default: enabled)
  bool shake_enabled = persist_exists(PERSIST_KEY_SHAKE_SECONDS)
    ? persist_read_bool(PERSIST_KEY_SHAKE_SECONDS)
    : true;
  hands_layer_set_shake_enabled(shake_enabled);

  // Apply persisted seconds-display duration (default: 10s)
  if (persist_exists(PERSIST_KEY_SECONDS_DURATION)) {
    uint32_t duration_ms = (uint32_t)persist_read_int(PERSIST_KEY_SECONDS_DURATION);
    hands_layer_set_seconds_duration(duration_ms);
  }

  // Apply persisted temperature unit preference (default: Celsius)
  bool fahrenheit = persist_exists(PERSIST_KEY_TEMPERATURE_UNIT)
    ? persist_read_bool(PERSIST_KEY_TEMPERATURE_UNIT)
    : false;
  weather_layer_set_fahrenheit(fahrenheit);

  // AppMessage — receive weather and settings from pkjs
  app_message_register_inbox_received(inbox_received_callback);
  app_message_open(128, 32);

  // Track phone connection so the weather widget can fall back to a
  // disconnect icon when unreachable.
  weather_layer_set_connected(connection_service_peek_pebble_app_connection());
  connection_service_subscribe((ConnectionHandlers){
    .pebble_app_connection_handler = connection_handler,
  });
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