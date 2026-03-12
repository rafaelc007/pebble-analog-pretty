#include "watchface.h"

// ============================================================================
// SHARED STATE DEFINITIONS
// These are declared extern in watchface.h — defined exactly once here
// ============================================================================

GPoint s_center;
int    s_radius;
int    s_w_radius;
int    s_h_radius;
GFont  s_font;

// ============================================================================
// GEOMETRY INIT — called once from main_window_load
// ============================================================================

void watchface_geometry_init(GRect bounds) {
  s_center   = grect_center_point(&bounds);
  s_w_radius = (bounds.size.w / 2) - 2;
  s_h_radius = (bounds.size.h / 2) - 2;
  s_radius   = (s_w_radius < s_h_radius) ? s_w_radius : s_h_radius;

  // Font cached here — shared by face and hands layers
  s_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
}

// ============================================================================
// GEOMETRY UTILITIES
// ============================================================================

int32_t degrees_to_trig_angle(int degrees) {
  return TRIG_MAX_ANGLE * degrees / 360;
}

GPoint get_point_on_circle(int32_t angle, int distance_from_center) {
  return (GPoint) {
    .x = s_center.x + (int)(sin_lookup(angle) * distance_from_center / TRIG_MAX_RATIO),
    .y = s_center.y - (int)(cos_lookup(angle) * distance_from_center / TRIG_MAX_RATIO)
  };
}

GPoint get_point_on_rect(int32_t angle, int w_radius, int h_radius) {
  int32_t sin_val = sin_lookup(angle);
  int32_t cos_val = cos_lookup(angle);
  int32_t abs_sin = sin_val > 0 ? sin_val : -sin_val;
  int32_t abs_cos = cos_val > 0 ? cos_val : -cos_val;
  int32_t scale;
  if (abs_sin * h_radius > abs_cos * w_radius) {
    scale = (int32_t)w_radius * TRIG_MAX_RATIO / abs_sin;
  } else {
    scale = (int32_t)h_radius * TRIG_MAX_RATIO / abs_cos;
  }
  return (GPoint) {
    .x = s_center.x + (int)(sin_val * scale / TRIG_MAX_RATIO),
    .y = s_center.y - (int)(cos_val * scale / TRIG_MAX_RATIO)
  };
}

GPoint get_point_on_face(int32_t angle, int w_dist, int h_dist) {
  #if defined(PBL_ROUND)
    return get_point_on_circle(angle, w_dist);
  #else
    return get_point_on_rect(angle, w_dist, h_dist);
  #endif
}

bool is_major_marker(int index) { return (index % MAJOR_MARKER_INTERVAL) == 0; }
int  get_display_hour(int index) { return (index == 0) ? 12 : index / 5; }