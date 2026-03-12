#include "layer_face.h"
#include "watchface.h"

// ============================================================================
// PRIVATE STATE — not visible outside this module
// ============================================================================

static Layer *s_face_layer;

// ============================================================================
// PRIVATE DRAWING FUNCTIONS
// ============================================================================

static void draw_clock_face(GContext *ctx) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, CLOCK_FACE_STROKE_WIDTH);
  #if defined(PBL_ROUND)
    graphics_draw_circle(ctx, s_center, s_radius);
  #else
    GRect rect = GRect(
      s_center.x - s_w_radius,
      s_center.y - s_h_radius,
      2 * s_w_radius,
      2 * s_h_radius
    );
    graphics_draw_round_rect(ctx, rect, 2);
  #endif
}

static void draw_marker(GContext *ctx, int index) {
  int32_t angle    = degrees_to_trig_angle(index * 6);
  bool    is_major = is_major_marker(index);
  int     len      = is_major ? MAJOR_MARKER_LENGTH : MINOR_MARKER_LENGTH;
  GPoint  outer    = get_point_on_face(angle, s_w_radius, s_h_radius);
  GPoint  inner    = get_point_on_face(angle, s_w_radius - len, s_h_radius - len);
  graphics_context_set_stroke_width(ctx, is_major ? MAJOR_MARKER_WIDTH : MINOR_MARKER_WIDTH);
  graphics_draw_line(ctx, outer, inner);
}

static void draw_hour_number(GContext *ctx, int index) {
  if (!is_major_marker(index)) return;
  int32_t angle  = degrees_to_trig_angle(index * 6);
  int     offset = MAJOR_MARKER_LENGTH + NUMBER_OFFSET_FROM_MARKER;
  GPoint  pos    = get_point_on_face(angle, s_w_radius - offset, s_h_radius - offset);
  static char buffer[3];
  snprintf(buffer, sizeof(buffer), "%d", get_display_hour(index));
  GRect text_rect = GRect(pos.x - 15, pos.y - 12, 30, 24);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, buffer, s_font,
                     text_rect, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void draw_all_markers(GContext *ctx) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  for (int i = 0; i < MINUTE_MARKER_COUNT; i++) {
    draw_marker(ctx, i);
    if (i % 5 == 0) draw_hour_number(ctx, i);
  }
}

// ============================================================================
// LAYER UPDATE PROC — static, called once by Pebble at layer creation
// ============================================================================

static void face_update_proc(Layer *layer, GContext *ctx) {
  draw_clock_face(ctx);
  draw_all_markers(ctx);
}

// ============================================================================
// PUBLIC API
// ============================================================================

Layer* face_layer_create(GRect bounds, Layer *parent) {
  s_face_layer = layer_create(bounds);
  layer_set_update_proc(s_face_layer, face_update_proc);
  layer_add_child(parent, s_face_layer);
  return s_face_layer;
}

void face_layer_destroy(void) {
  layer_destroy(s_face_layer);
  s_face_layer = NULL;
}