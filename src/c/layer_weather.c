#include "layer_weather.h"
#include "watchface.h"

// ============================================================================
// PRIVATE STATE
// ============================================================================

static Layer         *s_weather_layer;
static int            s_temp_c    = 0;
static WeatherIconType s_icon     = WeatherIconUnknown;
static bool           s_has_data  = false;

// ============================================================================
// ICON DRAWING — all primitives, no resources
// ============================================================================

// Draw a small cloud shape: two overlapping circles
// cx, cy = center of the cloud body
static void draw_cloud(GContext *ctx, int cx, int cy) {
  graphics_fill_circle(ctx, GPoint(cx - 3, cy + 2), 5);
  graphics_fill_circle(ctx, GPoint(cx + 4, cy + 2), 4);
  graphics_fill_circle(ctx, GPoint(cx,     cy - 1), 6);
}

static void draw_icon_clear(GContext *ctx, GPoint origin) {
  // Sun: filled circle + 8 short rays
  int cx = origin.x + 10;
  int cy = origin.y + 10;
  // Rays
  graphics_context_set_stroke_color(ctx, WATCHFACE_THEME_COLOR);
  graphics_context_set_stroke_width(ctx, 1);
  for (int d = 0; d < 360; d += 45) {
    int32_t angle = degrees_to_trig_angle(d);
    GPoint inner = {
      .x = cx + (int)(sin_lookup(angle) * 8 / TRIG_MAX_RATIO),
      .y = cy - (int)(cos_lookup(angle) * 8 / TRIG_MAX_RATIO)
    };
    GPoint outer = {
      .x = cx + (int)(sin_lookup(angle) * 11 / TRIG_MAX_RATIO),
      .y = cy - (int)(cos_lookup(angle) * 11 / TRIG_MAX_RATIO)
    };
    graphics_draw_line(ctx, inner, outer);
  }
  // Core
  graphics_context_set_fill_color(ctx, WATCHFACE_THEME_COLOR);
  graphics_fill_circle(ctx, GPoint(cx, cy), 5);
}

static void draw_icon_cloudy(GContext *ctx, GPoint origin) {
  int cx = origin.x + 10;
  int cy = origin.y + 11;
  graphics_context_set_fill_color(ctx, GColorLightGray);
  draw_cloud(ctx, cx, cy);
}

static void draw_icon_fog(GContext *ctx, GPoint origin) {
  int x0 = origin.x + 2;
  int y  = origin.y + 6;
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_context_set_stroke_width(ctx, 2);
  for (int i = 0; i < 3; i++) {
    graphics_draw_line(ctx, GPoint(x0, y + i * 4), GPoint(x0 + 15, y + i * 4));
  }
}

static void draw_icon_drizzle(GContext *ctx, GPoint origin) {
  int cx = origin.x + 10;
  int cy = origin.y + 7;
  graphics_context_set_fill_color(ctx, GColorLightGray);
  draw_cloud(ctx, cx, cy);
  // Two short drop lines below
  graphics_context_set_stroke_color(ctx, GColorCyan);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(cx - 3, cy + 7), GPoint(cx - 5, cy + 12));
  graphics_draw_line(ctx, GPoint(cx + 4, cy + 7), GPoint(cx + 2, cy + 12));
}

static void draw_icon_rain(GContext *ctx, GPoint origin) {
  int cx = origin.x + 10;
  int cy = origin.y + 6;
  graphics_context_set_fill_color(ctx, GColorLightGray);
  draw_cloud(ctx, cx, cy);
  // Three drop lines
  graphics_context_set_stroke_color(ctx, GColorCyan);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(cx - 4, cy + 7), GPoint(cx - 7, cy + 14));
  graphics_draw_line(ctx, GPoint(cx,     cy + 7), GPoint(cx - 3, cy + 14));
  graphics_draw_line(ctx, GPoint(cx + 4, cy + 7), GPoint(cx + 1, cy + 14));
}

static void draw_icon_snow(GContext *ctx, GPoint origin) {
  int cx = origin.x + 10;
  int cy = origin.y + 10;
  graphics_context_set_stroke_color(ctx, GColorCyan);
  graphics_context_set_stroke_width(ctx, 1);
  // 6-arm asterisk
  for (int d = 0; d < 180; d += 60) {
    int32_t angle = degrees_to_trig_angle(d);
    GPoint a = {
      .x = cx + (int)(sin_lookup(angle) * 8 / TRIG_MAX_RATIO),
      .y = cy - (int)(cos_lookup(angle) * 8 / TRIG_MAX_RATIO)
    };
    GPoint b = {
      .x = cx - (int)(sin_lookup(angle) * 8 / TRIG_MAX_RATIO),
      .y = cy + (int)(cos_lookup(angle) * 8 / TRIG_MAX_RATIO)
    };
    graphics_draw_line(ctx, a, b);
  }
  // Center dot
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, GPoint(cx, cy), 2);
}

static void draw_icon_storm(GContext *ctx, GPoint origin) {
  int cx = origin.x + 10;
  int cy = origin.y + 5;
  // Cloud
  graphics_context_set_fill_color(ctx, GColorLightGray);
  draw_cloud(ctx, cx, cy);
  // Lightning bolt (filled triangle-ish path via two lines with width)
  graphics_context_set_stroke_color(ctx, GColorYellow);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(cx,     cy + 7),  GPoint(cx - 4, cy + 13));
  graphics_draw_line(ctx, GPoint(cx - 4, cy + 13), GPoint(cx,     cy + 11));
  graphics_draw_line(ctx, GPoint(cx,     cy + 11), GPoint(cx - 5, cy + 18));
}

typedef void (*IconDrawFn)(GContext*, GPoint);

static const IconDrawFn s_icon_fns[] = {
  draw_icon_clear,   // 0
  draw_icon_cloudy,  // 1
  draw_icon_fog,     // 2
  draw_icon_drizzle, // 3
  draw_icon_rain,    // 4
  draw_icon_snow,    // 5
  draw_icon_storm,   // 6
  NULL,              // 7 unknown — no icon, just text
};

// ============================================================================
// LAYER UPDATE PROC
// ============================================================================

static void weather_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int w  = bounds.size.w;
  int lh = bounds.size.h;

  const int ICON_W = 20;
  const int ICON_H = 20;
  const int GAP    = 4;
  const int TEXT_H = 26;
  const int MAX_TEXT_W = 56; // enough for "-99°C"

  // Build text first so we can measure it
  static char temp_buf[8];
  if (s_has_data) {
    snprintf(temp_buf, sizeof(temp_buf), "%d" "\xc2\xb0" "C", s_temp_c);
  } else {
    snprintf(temp_buf, sizeof(temp_buf), "--" "\xc2\xb0" "C");
  }
  GFont weather_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  // Measure actual rendered text width so centering uses real content width
  GSize text_size = graphics_text_layout_get_content_size(
    temp_buf, weather_font,
    GRect(0, 0, MAX_TEXT_W, TEXT_H),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft
  );
  int text_w = text_size.w + 2; // +2 px safety margin

  // Center the full widget (icon + gap + text) on the X axis
  int total_w = ICON_W + GAP + text_w;
  int start_x = (w - total_w) / 2;
  int cy      = lh / 2;

  // Draw icon
  WeatherIconType icon = s_has_data ? s_icon : WeatherIconUnknown;
  if (icon < 7 && s_icon_fns[icon] != NULL) {
    GPoint icon_origin = GPoint(start_x, cy - ICON_H / 2);
    s_icon_fns[icon](ctx, icon_origin);
  }

  // Draw temperature text
  graphics_context_set_text_color(ctx, GColorWhite);
  GRect text_rect = GRect(
    start_x + ICON_W + GAP,
    cy - TEXT_H / 2,
    text_w,
    TEXT_H
  );
  graphics_draw_text(ctx, temp_buf, weather_font,
                     text_rect, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
}

// ============================================================================
// PUBLIC API
// ============================================================================

Layer* weather_layer_create(GRect bounds, Layer *parent) {
  // Place widget just below the 12 o'clock hour-number label, inside the
  // clock face interior — no overlap with ring, markers, or hour numbers.
  // Formula: top of 12 o'clock text = s_center.y - (face_h_edge - num_offset) - 16
  //          bottom of same text    =                                          + 16
  int face_h_edge  = s_h_radius - (CLOCK_FACE_STROKE_WIDTH / 2);
  int num_offset   = MAJOR_MARKER_LENGTH + s_num_offset;
  int label_bottom = s_center.y - (face_h_edge - num_offset) + 16;
  int layer_y      = label_bottom + 6;   // 6 px gap below the "12" label
  int layer_h      = 30;
  GRect layer_bounds = GRect(0, layer_y, bounds.size.w, layer_h);
  s_weather_layer = layer_create(layer_bounds);
  layer_set_update_proc(s_weather_layer, weather_update_proc);
  layer_add_child(parent, s_weather_layer);
  return s_weather_layer;
}

void weather_layer_set_data(int temp_c, WeatherIconType icon) {
  s_temp_c   = temp_c;
  s_icon     = icon;
  s_has_data = true;
  if (s_weather_layer) layer_mark_dirty(s_weather_layer);
}

void weather_layer_destroy(void) {
  if (s_weather_layer) {
    layer_destroy(s_weather_layer);
    s_weather_layer = NULL;
  }
}
