#include "layer_hands.h"
#include "watchface.h"

// ============================================================================
// PRIVATE STATE
// ============================================================================

static Layer    *s_hands_layer;
static bool      s_show_seconds   = false;
static bool      s_shake_enabled  = true;
static bool      s_accel_subscribed = false;
static bool      s_seconds_always_on = false;
static AppTimer *s_seconds_timer = NULL;
static uint32_t  s_seconds_duration = SECONDS_DISPLAY_DURATION_DEFAULT;

// ============================================================================
// PRIVATE: SECONDS TIMER MANAGEMENT
// ============================================================================

// Called when the 10-second display window expires
static void seconds_timer_callback(void *context) {
  s_seconds_timer = NULL;
  // Defensive: if always-on flipped on between scheduling and firing, leave state alone
  if (s_seconds_always_on) return;
  s_show_seconds  = false;

  // Stop per-second ticks — back to power-efficient minute ticks only
  tick_timer_service_subscribe(MINUTE_UNIT, (TickHandler)context);

  layer_mark_dirty(s_hands_layer);
}

// ============================================================================
// PRIVATE DRAWING FUNCTIONS
// ============================================================================

static void draw_clock_hands(GContext *ctx, struct tm *t) {
  static uint8_t hour_thickness = HOUR_HAND_WIDTH / 2;
  int32_t h_angle = degrees_to_trig_angle((t->tm_hour % 12) * 30 + (t->tm_min / 2));
  int32_t m_angle = degrees_to_trig_angle(t->tm_min * 6);
  GPoint  h_end   = get_point_on_circle(h_angle, s_hour_hand_len);
  GPoint  m_end   = get_point_on_circle(m_angle, s_minute_hand_len);
  GPoint  h_short = get_point_on_circle(h_angle, s_hour_hand_len - hour_thickness + 1);

  // Draw hour hand in two passes
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, HOUR_HAND_WIDTH);
  graphics_draw_line(ctx, s_center, h_end);

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, HOUR_HAND_WIDTH - hour_thickness);
  graphics_draw_line(ctx, s_center, h_short);

  // Draw minute hand
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, MINUTE_HAND_WIDTH);
  graphics_draw_line(ctx, s_center, m_end);

  // Draw seconds hand only when s_show_seconds is active
  if (s_show_seconds) {
    int32_t s_angle = degrees_to_trig_angle(t->tm_sec * 6);
    GPoint  s_end   = get_point_on_circle(s_angle, s_second_hand_len);
    GPoint  s_start = get_point_on_circle(revert_angle(s_angle), s_second_tail_len);
    graphics_context_set_stroke_color(ctx, WATCHFACE_THEME_COLOR);
    graphics_context_set_stroke_width(ctx, SECOND_HAND_WIDTH);
    graphics_draw_line(ctx, s_start, s_end);
  }

  // Center dot drawn last so it sits on top of all hands
  graphics_context_set_stroke_width(ctx, MINUTE_HAND_WIDTH);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_circle(ctx, s_center, CENTER_DOT_RADIUS);
  graphics_context_set_fill_color(ctx, WATCHFACE_THEME_COLOR);
  graphics_fill_circle(ctx, s_center, CENTER_DOT_RADIUS - 2);
}

// ============================================================================
// LAYER UPDATE PROC
// ============================================================================

static void hands_update_proc(Layer *layer, GContext *ctx) {
  time_t    now = time(NULL);
  struct tm *t  = localtime(&now);

  graphics_context_set_antialiased(ctx, false);
  draw_clock_hands(ctx, t);
}

// ============================================================================
// PUBLIC API
// ============================================================================

Layer* hands_layer_create(GRect bounds, Layer *parent) {
  s_hands_layer = layer_create(bounds);
  layer_set_update_proc(s_hands_layer, hands_update_proc);
  layer_add_child(parent, s_hands_layer);
  return s_hands_layer;
}

void hands_layer_mark_dirty(void) {
  if (s_hands_layer) layer_mark_dirty(s_hands_layer);
}

void hands_layer_destroy(void) {
  // Cancel any active timer before destroying the layer
  if (s_seconds_timer) {
    app_timer_cancel(s_seconds_timer);
    s_seconds_timer = NULL;
  }
  layer_destroy(s_hands_layer);
  s_hands_layer = NULL;
}

// Tap handler — registered via accel_tap_service_subscribe when shake-to-show is enabled
void hands_layer_handle_tap(AccelAxisType axis, int32_t direction) {
  if (s_seconds_always_on) return;
  if (!s_shake_enabled) return;

  // If seconds are already showing, reset the countdown timer
  if (s_seconds_timer) {
    app_timer_reschedule(s_seconds_timer, s_seconds_duration);
    return;
  }

  // First shake — activate seconds display
  s_show_seconds = true;

  // Switch from MINUTE_UNIT to SECOND_UNIT while seconds are visible
  // Pass the tick_handler from main.c via context so we can restore it
  extern void tick_handler(struct tm*, TimeUnits);
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);

  // Schedule auto-hide after the user-configured duration
  s_seconds_timer = app_timer_register(
    s_seconds_duration,
    seconds_timer_callback,
    (void*)tick_handler   // pass tick_handler so callback can restore it
  );

  layer_mark_dirty(s_hands_layer);
}

void hands_layer_set_seconds_always_on(bool enabled) {
  if (enabled == s_seconds_always_on) return;
  s_seconds_always_on = enabled;

  extern void tick_handler(struct tm*, TimeUnits);

  if (enabled) {
    // Cancel any pending shake-window timer
    if (s_seconds_timer) {
      app_timer_cancel(s_seconds_timer);
      s_seconds_timer = NULL;
    }
    // Shake is redundant while seconds are always visible — drop the accel
    // subscription for the power saving this mode is meant to allow.
    if (s_accel_subscribed) {
      accel_tap_service_unsubscribe();
      s_accel_subscribed = false;
    }
    s_show_seconds = true;
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  } else {
    s_show_seconds = false;
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    // Restore accel subscription if the user still wants shake-to-show
    if (s_shake_enabled && !s_accel_subscribed) {
      accel_tap_service_subscribe(hands_layer_handle_tap);
      s_accel_subscribed = true;
    }
  }

  if (s_hands_layer) layer_mark_dirty(s_hands_layer);
}

void hands_layer_set_seconds_duration(uint32_t duration_ms) {
  if (duration_ms < SECONDS_DISPLAY_DURATION_MIN) duration_ms = SECONDS_DISPLAY_DURATION_MIN;
  if (duration_ms > SECONDS_DISPLAY_DURATION_MAX) duration_ms = SECONDS_DISPLAY_DURATION_MAX;
  s_seconds_duration = duration_ms;
  // If a countdown is already running, extend/shrink it to match
  if (s_seconds_timer) {
    app_timer_reschedule(s_seconds_timer, s_seconds_duration);
  }
}

void hands_layer_set_shake_enabled(bool enabled) {
  if (enabled == s_shake_enabled && enabled == s_accel_subscribed) return;
  s_shake_enabled = enabled;

  // While always-on owns the accel/tick state, just record the preference.
  if (s_seconds_always_on) return;

  if (enabled && !s_accel_subscribed) {
    accel_tap_service_subscribe(hands_layer_handle_tap);
    s_accel_subscribed = true;
  } else if (!enabled && s_accel_subscribed) {
    accel_tap_service_unsubscribe();
    s_accel_subscribed = false;

    // If seconds are currently showing, hide them immediately
    if (s_seconds_timer) {
      app_timer_cancel(s_seconds_timer);
      s_seconds_timer = NULL;
    }
    if (s_show_seconds) {
      s_show_seconds = false;
      extern void tick_handler(struct tm*, TimeUnits);
      tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
      if (s_hands_layer) layer_mark_dirty(s_hands_layer);
    }
  }
}