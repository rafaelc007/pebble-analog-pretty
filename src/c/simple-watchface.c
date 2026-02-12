#include <pebble.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define CLOCK_FACE_BORDER_PERCENT 0.01f
#define CLOCK_FACE_CORNER_RADIUS 7
#define CLOCK_FACE_STROKE_WIDTH 2

#define HOUR_MARKER_COUNT 12
#define MAJOR_MARKER_INTERVAL 3
#define MAJOR_MARKER_LENGTH 15
#define MINOR_MARKER_LENGTH 8
#define MAJOR_MARKER_WIDTH 3
#define MINOR_MARKER_WIDTH 1
#define NUMBER_OFFSET_FROM_MARKER 10

#define HOUR_HAND_LENGTH_RATIO 0.5f
#define MINUTE_HAND_LENGTH_RATIO 0.75f
#define SECOND_HAND_LENGTH_RATIO 0.85f

#define HOUR_HAND_WIDTH 5
#define MINUTE_HAND_WIDTH 3
#define SECOND_HAND_WIDTH 1

#define CENTER_DOT_RADIUS 5

#define DEGREES_PER_HOUR 30
#define DEGREES_PER_MINUTE 6
#define DEGREES_PER_SECOND 6
#define DEGREES_PER_MINUTE_FOR_HOUR_HAND 0.5f

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

/**
 * Converts degrees to Pebble's trigonometric angle format
 */
static int32_t degrees_to_trig_angle(int degrees) {
  return TRIG_MAX_ANGLE * degrees / 360;
}

/**
 * Calculates a point on the clock face at a given angle and distance
 */
static GPoint get_point_on_circle(int32_t angle, int distance_from_center) {
  return (GPoint) {
    .x = s_center.x + (int)(sin_lookup(angle) * distance_from_center / TRIG_MAX_RATIO),
    .y = s_center.y - (int)(cos_lookup(angle) * distance_from_center / TRIG_MAX_RATIO)
  };
}

/**
 * Calculates a point on an ellipse at a given angle
 */
static GPoint get_point_on_ellipse(int32_t angle, int w_radius, int h_radius) {
  return (GPoint) {
    .x = s_center.x + (int)(sin_lookup(angle) * w_radius / TRIG_MAX_RATIO),
    .y = s_center.y - (int)(cos_lookup(angle) * h_radius / TRIG_MAX_RATIO)
  };
}

/**
 * Determines if an hour marker should be a major marker (12, 3, 6, 9)
 */
static bool is_major_marker(int hour_index) {
  return (hour_index % MAJOR_MARKER_INTERVAL) == 0;
}

/**
 * Converts hour index (0-11) to display hour (1-12)
 */
static int get_display_hour(int hour_index) {
  return (hour_index == 0) ? 12 : hour_index;
}

// ============================================================================
// DRAWING FUNCTIONS
// ============================================================================

/**
 * Draws the clock face border
 */
static void draw_clock_face(GContext *ctx) {
  GRect clock_face = GRect(
    s_center.x - s_w_radius,
    s_center.y - s_h_radius,
    2 * s_w_radius,
    2 * s_h_radius
  );
  
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, CLOCK_FACE_STROKE_WIDTH);
  graphics_draw_round_rect(ctx, clock_face, CLOCK_FACE_CORNER_RADIUS);
}

/**
 * Draws a single hour marker
 */
static void draw_hour_marker(GContext *ctx, int hour_index) {
  int angle_degrees = hour_index * DEGREES_PER_HOUR;
  int32_t angle = degrees_to_trig_angle(angle_degrees);
  
  bool is_major = is_major_marker(hour_index);
  int marker_length = is_major ? MAJOR_MARKER_LENGTH : MINOR_MARKER_LENGTH;
  int marker_width = is_major ? MAJOR_MARKER_WIDTH : MINOR_MARKER_WIDTH;
  
  GPoint outer = get_point_on_ellipse(angle, s_w_radius, s_h_radius);
  GPoint inner = get_point_on_ellipse(angle, s_w_radius - marker_length, s_h_radius - marker_length);
  
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, marker_width);
  graphics_draw_line(ctx, outer, inner);
}

/**
 * Draws hour numbers at major markers (12, 3, 6, 9)
 */
static void draw_hour_number(GContext *ctx, int hour_index) {
  if (!is_major_marker(hour_index)) {
    return;
  }
  
  int angle_degrees = hour_index * DEGREES_PER_HOUR;
  int32_t angle = degrees_to_trig_angle(angle_degrees);
  
  int number_distance = s_radius - MAJOR_MARKER_LENGTH - NUMBER_OFFSET_FROM_MARKER;
  GPoint number_pos = get_point_on_circle(angle, number_distance);
  
  // Format number
  static char number_buffer[3];
  snprintf(number_buffer, sizeof(number_buffer), "%d", get_display_hour(hour_index));
  
  // Calculate text dimensions for centering
  GFont number_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GSize text_size = graphics_text_layout_get_content_size(
    number_buffer,
    number_font,
    GRect(0, 0, 30, 30),
    GTextOverflowModeWordWrap,
    GTextAlignmentCenter
  );
  
  // Draw centered text
  GRect text_rect = GRect(
    number_pos.x - text_size.w / 2,
    number_pos.y - text_size.h / 2,
    text_size.w,
    text_size.h
  );
  
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(
    ctx,
    number_buffer,
    number_font,
    text_rect,
    GTextOverflowModeWordWrap,
    GTextAlignmentCenter,
    NULL
  );
}

/**
 * Draws all hour markers and numbers
 */
static void draw_hour_markers(GContext *ctx) {
  for (int i = 0; i < HOUR_MARKER_COUNT; i++) {
    draw_hour_marker(ctx, i);
    draw_hour_number(ctx, i);
  }
}

/**
 * Draws a clock hand
 */
static void draw_hand(GContext *ctx, int32_t angle, float length_ratio, int width, GColor color) {
  GPoint hand_end = get_point_on_circle(angle, (int)(s_radius * length_ratio));
  
  graphics_context_set_stroke_width(ctx, width);
  graphics_context_set_stroke_color(ctx, color);
  graphics_draw_line(ctx, s_center, hand_end);
}

/**
 * Calculates and draws all clock hands based on current time
 */
static void draw_clock_hands(GContext *ctx, struct tm *t) {
  // Calculate angles
  int hour_degrees = (t->tm_hour % 12) * DEGREES_PER_HOUR + 
                     (int)(t->tm_min * DEGREES_PER_MINUTE_FOR_HOUR_HAND);
  int minute_degrees = t->tm_min * DEGREES_PER_MINUTE;
  int second_degrees = t->tm_sec * DEGREES_PER_SECOND;
  
  int32_t hour_angle = degrees_to_trig_angle(hour_degrees);
  int32_t minute_angle = degrees_to_trig_angle(minute_degrees);
  int32_t second_angle = degrees_to_trig_angle(second_degrees);
  
  // Draw hands (back to front)
  draw_hand(ctx, hour_angle, HOUR_HAND_LENGTH_RATIO, HOUR_HAND_WIDTH, GColorWhite);
  draw_hand(ctx, minute_angle, MINUTE_HAND_LENGTH_RATIO, MINUTE_HAND_WIDTH, GColorWhite);
  draw_hand(ctx, second_angle, SECOND_HAND_LENGTH_RATIO, SECOND_HAND_WIDTH, GColorRed);
  
  // Draw center dot
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, s_center, CENTER_DOT_RADIUS);
}

/**
 * Initializes drawing dimensions based on layer bounds
 */
static void calculate_dimensions(Layer *layer) {
  GRect bounds = layer_get_bounds(layer);
  s_center = grect_center_point(&bounds);
  
  s_w_radius = (int)(bounds.size.w / 2 * (1 - CLOCK_FACE_BORDER_PERCENT));
  s_h_radius = (int)(bounds.size.h / 2 * (1 - CLOCK_FACE_BORDER_PERCENT));
  s_radius = (s_w_radius < s_h_radius) ? s_w_radius : s_h_radius;
}

// ============================================================================
// LAYER UPDATE CALLBACK
// ============================================================================

/**
 * Main update procedure for the canvas layer
 */
static void update_proc(Layer *layer, GContext *ctx) {
  calculate_dimensions(layer);
  
  graphics_context_set_fill_color(ctx, GColorWhite);
  
  draw_clock_face(ctx);
  draw_hour_markers(ctx);
  
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  draw_clock_hands(ctx, t);
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

/**
 * Called every second to update the display
 */
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(s_canvas_layer);
}

/**
 * Called when the main window is loaded
 */
static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, update_proc);
  layer_add_child(window_layer, s_canvas_layer);
}

/**
 * Called when the main window is unloaded
 */
static void main_window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
}

// ============================================================================
// LIFECYCLE FUNCTIONS
// ============================================================================

/**
 * Initializes the application
 */
static void init(void) {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  
  window_stack_push(s_main_window, true);
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

/**
 * Cleans up resources on exit
 */
static void deinit(void) {
  window_destroy(s_main_window);
}

/**
 * Application entry point
 */
int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}