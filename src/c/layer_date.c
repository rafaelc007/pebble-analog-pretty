#include "layer_date.h"
#include "watchface.h"

static Layer *s_date_layer;

static void date_update_proc(Layer *layer, GContext *ctx) {
  static const char * const WEEKDAYS[] = {
    "SUN","MON","TUE","WED","THU","FRI","SAT"
  };
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  static char date_buffer[8]; // "SAT-31\0"
  snprintf(date_buffer, sizeof(date_buffer), "%s-%d", WEEKDAYS[t->tm_wday], t->tm_mday);

  GRect bounds = layer_get_bounds(layer);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(WATCHFACE_THEME_COLOR, GColorWhite));
  graphics_draw_text(ctx, date_buffer, font,
                     bounds, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

Layer* date_layer_create(GRect bounds, Layer *parent) {
  // Centered horizontally, sitting between the center dot and the "6" label
  int face_h_edge = s_h_radius - (CLOCK_FACE_STROKE_WIDTH / 2);
  int num_offset  = MAJOR_MARKER_LENGTH + s_num_offset;
  int six_top     = s_center.y + (face_h_edge - num_offset) - 16;
  int dot_bottom  = s_center.y + CENTER_DOT_RADIUS;
  int mid_y       = (dot_bottom + six_top) / 2;

  GRect frame = GRect(s_center.x - 50, mid_y - 15, 100, 30);
  s_date_layer = layer_create(frame);
  layer_set_update_proc(s_date_layer, date_update_proc);
  layer_add_child(parent, s_date_layer);
  return s_date_layer;
}

void date_layer_mark_dirty(void) {
  if (s_date_layer) layer_mark_dirty(s_date_layer);
}

void date_layer_destroy(void) {
  if (s_date_layer) {
    layer_destroy(s_date_layer);
    s_date_layer = NULL;
  }
}
