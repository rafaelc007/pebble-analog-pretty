#include <pebble.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define CLOCK_FACE_STROKE_WIDTH 2
#define MINUTE_MARKER_COUNT 60
#define MAJOR_MARKER_INTERVAL 5
#define MAJOR_MARKER_LENGTH 12
#define MINOR_MARKER_LENGTH 5
#define MAJOR_MARKER_WIDTH 3
#define MINOR_MARKER_WIDTH 1
#define NUMBER_OFFSET_FROM_MARKER 12

// New constant for date positioning
#define DATE_OFFSET_FROM_CENTER 40 

#define HOUR_HAND_LENGTH_RATIO 0.5f
#define MINUTE_HAND_LENGTH_RATIO 0.75f
#define SECOND_HAND_LENGTH_RATIO 0.85f

#define HOUR_HAND_WIDTH 5
#define MINUTE_HAND_WIDTH 4
#define SECOND_HAND_WIDTH 2
#define CENTER_DOT_RADIUS 4

// ============================================================================
// GLOBAL STATE
// ============================================================================

static Window *s_main_window;
static Layer *s_canvas_layer;
static GPoint s_center;
static int s_radius;
static int s_w_radius;
static int s_h_radius;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static int32_t degrees_to_trig_angle(int degrees) {
  return TRIG_MAX_ANGLE * degrees / 360;
}

static GPoint get_point_on_circle(int32_t angle, int distance_from_center) {
  return (GPoint) {
    .x = s_center.x + (int)(sin_lookup(angle) * distance_from_center / TRIG_MAX_RATIO),
    .y = s_center.y - (int)(cos_lookup(angle) * distance_from_center / TRIG_MAX_RATIO)
  };
}

static GPoint get_point_on_rect(int32_t angle, int w_radius, int h_radius) {
  int32_t sin = sin_lookup(angle);
  int32_t cos = cos_lookup(angle);
  int32_t abs_sin = sin > 0 ? sin : -sin;
  int32_t abs_cos = cos > 0 ? cos : -cos;
  int32_t scale;
  if (abs_sin * h_radius > abs_cos * w_radius) {
    scale = (int32_t)w_radius * TRIG_MAX_RATIO / abs_sin;
  } else {
    scale = (int32_t)h_radius * TRIG_MAX_RATIO / abs_cos;
  }
  return (GPoint) {
    .x = s_center.x + (int)(sin * scale / TRIG_MAX_RATIO),
    .y = s_center.y - (int)(cos * scale / TRIG_MAX_RATIO)
  };
}

static GPoint get_point_on_face(int32_t angle, int w_dist, int h_dist) {
  #if defined(PBL_ROUND)
    return get_point_on_circle(angle, w_dist);
  #else
    return get_point_on_rect(angle, w_dist, h_dist);
  #endif
}

static bool is_major_marker(int index) { return (index % MAJOR_MARKER_INTERVAL) == 0; }
static int get_display_hour(int index) { return (index == 0) ? 12 : index / 5; }

// ============================================================================
// DRAWING FUNCTIONS
// ============================================================================

/**
 * NEW: Draws the date number to the left of the 3 o'clock position
 */
static void draw_date_widget(GContext *ctx, struct tm *t) {
  // 3 o'clock is 90 degrees
  int32_t angle = degrees_to_trig_angle(90);
  
  // Calculate position: move inward from the number 3
  // We use a fixed distance from the center for the date
  int date_dist_w = s_w_radius - (MAJOR_MARKER_LENGTH + NUMBER_OFFSET_FROM_MARKER + 25);
  int date_dist_h = s_h_radius - (MAJOR_MARKER_LENGTH + NUMBER_OFFSET_FROM_MARKER + 25);
  
  GPoint pos = get_point_on_face(angle, date_dist_w, date_dist_h);
  
  static char date_buffer[3];
  snprintf(date_buffer, sizeof(date_buffer), "%d", t->tm_mday);
  
  // Set color: Blue on color watches, White on B&W
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorBlue, GColorWhite));
  
  GRect text_rect = GRect(pos.x - 15, pos.y - 12, 30, 24);
  graphics_draw_text(ctx, date_buffer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), 
                     text_rect, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void draw_clock_face(GContext *ctx) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, CLOCK_FACE_STROKE_WIDTH);
  #if defined(PBL_ROUND)
    graphics_draw_circle(ctx, s_center, s_radius);
  #else
    GRect rect = GRect(s_center.x - s_w_radius, s_center.y - s_h_radius, 2 * s_w_radius, 2 * s_h_radius);
    graphics_draw_round_rect(ctx, rect, 2);
  #endif
}

static void draw_marker(GContext *ctx, int index) {
  int32_t angle = degrees_to_trig_angle(index * 6);
  bool is_major = is_major_marker(index);
  int len = is_major ? MAJOR_MARKER_LENGTH : MINOR_MARKER_LENGTH;
  GPoint outer = get_point_on_face(angle, s_w_radius, s_h_radius);
  GPoint inner = get_point_on_face(angle, s_w_radius - len, s_h_radius - len);
  graphics_context_set_stroke_width(ctx, is_major ? MAJOR_MARKER_WIDTH : MINOR_MARKER_WIDTH);
  graphics_draw_line(ctx, outer, inner);
}

static void draw_hour_number(GContext *ctx, int index) {
  if (!is_major_marker(index)) return;
  int32_t angle = degrees_to_trig_angle(index * 6);
  int offset = MAJOR_MARKER_LENGTH + NUMBER_OFFSET_FROM_MARKER;
  GPoint pos = get_point_on_face(angle, s_w_radius - offset, s_h_radius - offset);
  static char buffer[3];
  snprintf(buffer, sizeof(buffer), "%d", get_display_hour(index));
  GRect text_rect = GRect(pos.x - 15, pos.y - 12, 30, 24);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, buffer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), 
                     text_rect, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void draw_all_markers(GContext *ctx) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  for (int i = 0; i < MINUTE_MARKER_COUNT; i++) {
    draw_marker(ctx, i);
    if (i % 5 == 0) draw_hour_number(ctx, i);
  }
}

static void draw_clock_hands(GContext *ctx, struct tm *t) {
  int32_t h_angle = degrees_to_trig_angle((t->tm_hour % 12) * 30 + (t->tm_min / 2));
  int32_t m_angle = degrees_to_trig_angle(t->tm_min * 6);
  int32_t s_angle = degrees_to_trig_angle(t->tm_sec * 6);
  GPoint h_end = get_point_on_circle(h_angle, s_radius * HOUR_HAND_LENGTH_RATIO);
  GPoint m_end = get_point_on_circle(m_angle, s_radius * MINUTE_HAND_LENGTH_RATIO);
  GPoint s_end = get_point_on_circle(s_angle, s_radius * SECOND_HAND_LENGTH_RATIO);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, HOUR_HAND_WIDTH);
  graphics_draw_line(ctx, s_center, h_end);
  graphics_context_set_stroke_width(ctx, MINUTE_HAND_WIDTH);
  graphics_draw_line(ctx, s_center, m_end);
  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_context_set_stroke_width(ctx, SECOND_HAND_WIDTH);
  graphics_draw_line(ctx, s_center, s_end);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, s_center, CENTER_DOT_RADIUS);
}

// ============================================================================
// ENGINE
// ============================================================================

static void update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  s_center = grect_center_point(&bounds);
  s_w_radius = (bounds.size.w / 2) - 2;
  s_h_radius = (bounds.size.h / 2) - 2;
  s_radius = (s_w_radius < s_h_radius) ? s_w_radius : s_h_radius;

  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  draw_clock_face(ctx);
  draw_all_markers(ctx);
  draw_date_widget(ctx, t); // Draw the date
  draw_clock_hands(ctx, t);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) { layer_mark_dirty(s_canvas_layer); }

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas_layer, update_proc);
  layer_add_child(root, s_canvas_layer);
}

static void main_window_unload(Window *window) { layer_destroy(s_canvas_layer); }

static void init(void) {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_window_handlers(s_main_window, (WindowHandlers) {.load = main_window_load, .unload = main_window_unload});
  window_stack_push(s_main_window, true);
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

static void deinit(void) { window_destroy(s_main_window); }

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}