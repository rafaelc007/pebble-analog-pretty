#include "layer_hands.h"
#include "watchface.h"

// ============================================================================
// PRIVATE STATE
// ============================================================================

static Layer *s_hands_layer;

// ============================================================================
// PRIVATE DRAWING FUNCTIONS
// ============================================================================

static void draw_date_widget(GContext *ctx, struct tm *t) {
  int32_t angle       = degrees_to_trig_angle(90);
  int     date_dist_w = s_w_radius - (MAJOR_MARKER_LENGTH + NUMBER_OFFSET_FROM_MARKER + 25);
  int     date_dist_h = s_h_radius - (MAJOR_MARKER_LENGTH + NUMBER_OFFSET_FROM_MARKER + 25);
  GPoint  pos         = get_point_on_face(angle, date_dist_w, date_dist_h);
  static char date_buffer[3];
  snprintf(date_buffer, sizeof(date_buffer), "%d", t->tm_mday);
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorBlue, GColorWhite));
  GRect text_rect = GRect(pos.x - 15, pos.y - 12, 30, 24);
  graphics_draw_text(ctx, date_buffer, s_font,
                     text_rect, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void draw_clock_hands(GContext *ctx, struct tm *t) {
  int32_t h_angle = degrees_to_trig_angle((t->tm_hour % 12) * 30 + (t->tm_min / 2));
  int32_t m_angle = degrees_to_trig_angle(t->tm_min * 6);
  GPoint  h_end   = get_point_on_circle(h_angle, s_radius * HOUR_HAND_LENGTH_RATIO);
  GPoint  m_end   = get_point_on_circle(m_angle, s_radius * MINUTE_HAND_LENGTH_RATIO);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, HOUR_HAND_WIDTH);
  graphics_draw_line(ctx, s_center, h_end);
  graphics_context_set_stroke_width(ctx, MINUTE_HAND_WIDTH);
  graphics_draw_line(ctx, s_center, m_end);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, s_center, CENTER_DOT_RADIUS);
}

// ============================================================================
// LAYER UPDATE PROC
// ============================================================================

static void hands_update_proc(Layer *layer, GContext *ctx) {
  time_t    now = time(NULL);
  struct tm *t  = localtime(&now);
  draw_date_widget(ctx, t);
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
  layer_mark_dirty(s_hands_layer);
}

void hands_layer_destroy(void) {
  layer_destroy(s_hands_layer);
  s_hands_layer = NULL;
}