#pragma once
#include <pebble.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define CLOCK_FACE_STROKE_WIDTH    2
#define MINUTE_MARKER_COUNT        60
#define MAJOR_MARKER_INTERVAL      5
#define MAJOR_MARKER_LENGTH        12
#define MINOR_MARKER_LENGTH        5
#define MAJOR_MARKER_WIDTH         3
#define MINOR_MARKER_WIDTH         1
#define NUMBER_OFFSET_FROM_MARKER  12
#define DATE_OFFSET_FROM_CENTER    40

#define HOUR_HAND_LENGTH_RATIO     0.5f
#define MINUTE_HAND_LENGTH_RATIO   0.75f

#define HOUR_HAND_WIDTH            5
#define MINUTE_HAND_WIDTH          4
#define CENTER_DOT_RADIUS          4

// ============================================================================
// SHARED GEOMETRY STATE — owned by watchface.c, read by all modules
// ============================================================================

// These are extern so all modules can read cached geometry
// without recomputing or passing parameters everywhere
extern GPoint s_center;
extern int    s_radius;
extern int    s_w_radius;
extern int    s_h_radius;
extern GFont  s_font;

// ============================================================================
// GEOMETRY API
// ============================================================================

// Call once at window load to cache all shared geometry
void watchface_geometry_init(GRect bounds);

int32_t degrees_to_trig_angle(int degrees);

GPoint get_point_on_circle(int32_t angle, int distance_from_center);
GPoint get_point_on_rect(int32_t angle, int w_radius, int h_radius);
GPoint get_point_on_face(int32_t angle, int w_dist, int h_dist);

bool is_major_marker(int index);
int  get_display_hour(int index);