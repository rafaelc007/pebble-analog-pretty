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
GFont  date_font;

// ============================================================================
// GEOMETRY INIT 
// ============================================================================

void watchface_geometry_init(GRect bounds) {
  s_center   = grect_center_point(&bounds);
  s_w_radius = (bounds.size.w / 2) - 2;
  s_h_radius = (bounds.size.h / 2) - 2;
  s_radius   = (s_w_radius < s_h_radius) ? s_w_radius : s_h_radius;

  // Font cached here — shared by face and hands layers
  s_font = fonts_get_system_font(FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM);
  date_font = fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS);
}

// ============================================================================
// GEOMETRY UTILITIES
// ============================================================================

int32_t degrees_to_trig_angle(int degrees) {
  return TRIG_MAX_ANGLE * degrees / 360;
}

int32_t revert_angle(int32_t angle) {
  return angle + 32768;
}

GPoint get_point_on_circle(int32_t angle, int distance_from_center) {
  return (GPoint) {
    .x = s_center.x + (int)(sin_lookup(angle) * distance_from_center / TRIG_MAX_RATIO),
    .y = s_center.y - (int)(cos_lookup(angle) * distance_from_center / TRIG_MAX_RATIO)
  };
}

static int64_t isqrt(int64_t n) {
  if (n < 0) return 0;
  if (n == 0) return 0;
  int64_t x = n;
  int64_t y = (x + 1) / 2;
  while (y < x) {
    x = y;
    y = (x + n / x) / 2;
  }
  return x;
}

GPoint get_point_on_rounded_rect(int32_t angle, int w_radius, int h_radius, int corner_radius) {
  int32_t sin_val = sin_lookup(angle);
  int32_t cos_val = cos_lookup(angle);

  int cx = w_radius - corner_radius;  // x distance from center to corner arc center
  int cy = h_radius - corner_radius;  // y distance from center to corner arc center

  int32_t abs_sin = sin_val > 0 ? sin_val : -sin_val;
  int32_t abs_cos = cos_val > 0 ? cos_val : -cos_val;

  int32_t scale;
  if (abs_sin * h_radius > abs_cos * w_radius) {
    scale = (int32_t)w_radius * TRIG_MAX_RATIO / abs_sin;
  } else {
    scale = (int32_t)h_radius * TRIG_MAX_RATIO / abs_cos;
  }

  int px = (int)(sin_val * scale / TRIG_MAX_RATIO);
  int py = -(int)(cos_val * scale / TRIG_MAX_RATIO);

  bool in_corner = (px > cx || px < -cx) && (py > cy || py < -cy);

  if (!in_corner) {
    return (GPoint) {
      .x = s_center.x + px,
      .y = s_center.y + py
    };
  }

  int arc_cx = (sin_val > 0) ?  cx : -cx;
  int arc_cy = (cos_val > 0) ? -cy :  cy;

  int64_t dot   = (int64_t)arc_cx * sin_val + (int64_t)arc_cy * (-cos_val);
  int64_t cross = (int64_t)arc_cx * (-cos_val) - (int64_t)arc_cy * sin_val;

  int64_t r_trig    = (int64_t)corner_radius * TRIG_MAX_RATIO;
  int64_t discriminant = r_trig * r_trig - cross * cross;

  if (discriminant < 0) discriminant = 0;

  int64_t sqrt_disc = isqrt(discriminant);

  int64_t t = (dot + sqrt_disc) / TRIG_MAX_RATIO;

  return (GPoint) {
    .x = s_center.x + (int)(sin_val * t / TRIG_MAX_RATIO),
    .y = s_center.y - (int)(cos_val * t / TRIG_MAX_RATIO)
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
    return get_point_on_rounded_rect(angle, w_dist, h_dist, SQR_WATCHFACE_RADIOUS);
  #endif
}

bool is_major_marker(int index) { return (index % MAJOR_MARKER_INTERVAL) == 0; }
int  get_display_hour(int index) { return (index == 0) ? 12 : index / 5; }