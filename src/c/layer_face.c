#include "layer_face.h"
#include "watchface.h"

// ============================================================================
// PRIVATE STATE — not visible outside this module
// ============================================================================

static Layer *s_face_layer;
static int s_active_hour = -1;

// Precomputed marker geometry — populated once in face_layer_create.
typedef struct {
  GPoint outer;
  GPoint inner;
  bool   is_major;
} MarkerPoints;

static MarkerPoints s_markers[MINUTE_MARKER_COUNT];

// Precomputed hour-number label rects and label strings (12 labels).
static GRect        s_hour_rects[12];
static const char  *s_hour_labels[12] = {
  "12","1","2","3","4","5","6","7","8","9","10","11"
};

// ============================================================================
// PRIVATE: PRECOMPUTE GEOMETRY
// ============================================================================

static void precompute_face_geometry(void) {
  int face_w  = s_w_radius - (CLOCK_FACE_STROKE_WIDTH / 2);
  int face_h  = s_h_radius - (CLOCK_FACE_STROKE_WIDTH / 2);
  int offset  = MAJOR_MARKER_LENGTH + s_num_offset;

  for (int i = 0; i < MINUTE_MARKER_COUNT; i++) {
    int32_t angle    = degrees_to_trig_angle(i * 6);
    bool    is_major = is_major_marker(i);
    int     len      = is_major ? MAJOR_MARKER_LENGTH : MINOR_MARKER_LENGTH;

    s_markers[i].outer    = get_point_on_face(angle, face_w, face_h);
    s_markers[i].inner    = get_point_on_face(angle, face_w - len, face_h - len);
    s_markers[i].is_major = is_major;

    // Hour label rect for major markers (index 0,5,10,...55 → hours 12,1,...11)
    if (is_major) {
      GPoint pos = get_point_on_face(angle, face_w - offset, face_h - offset);
      int label_idx = i / 5;  // 0..11
      s_hour_rects[label_idx] = GRect(pos.x - 16, pos.y - 16, 32, 32);
    }
  }
}

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
    graphics_draw_round_rect(ctx, rect, SQR_WATCHFACE_RADIOUS);
  #endif
}

static void draw_all_markers(GContext *ctx) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  for (int i = 0; i < MINUTE_MARKER_COUNT; i++) {
    graphics_context_set_stroke_width(ctx,
      s_markers[i].is_major ? MAJOR_MARKER_WIDTH : MINOR_MARKER_WIDTH);
    graphics_draw_line(ctx, s_markers[i].outer, s_markers[i].inner);
  }

  for (int li = 0; li < 12; li++) {
    int hour = (li == 0) ? 12 : li;  // label_idx 0→"12", 1→"1", …, 11→"11"
    bool is_active = (hour == s_active_hour);

    #if defined(PBL_COLOR)
    graphics_context_set_text_color(ctx,
      is_active ? HOUR_NUMBER_ACTIVE_COLOR : HOUR_NUMBER_INACTIVE_COLOR);
    #else
    (void)is_active;
    graphics_context_set_text_color(ctx, HOUR_NUMBER_ACTIVE_COLOR);
    #endif

    graphics_draw_text(ctx, s_hour_labels[li], s_font,
                       s_hour_rects[li], GTextOverflowModeWordWrap,
                       GTextAlignmentCenter, NULL);
  }
}

// ============================================================================
// LAYER UPDATE PROC
// ============================================================================

static void face_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_antialiased(ctx, false);
  draw_clock_face(ctx);
  draw_all_markers(ctx);
}

// ============================================================================
// PUBLIC API
// ============================================================================

Layer* face_layer_create(GRect bounds, Layer *parent) {
  precompute_face_geometry();
  s_face_layer = layer_create(bounds);
  layer_set_update_proc(s_face_layer, face_update_proc);
  layer_add_child(parent, s_face_layer);
  return s_face_layer;
}

void face_layer_destroy(void) {
  layer_destroy(s_face_layer);
  s_face_layer = NULL;
}

bool face_layer_update_hour(int current_hour) {
  int display_hour = current_hour % 12;
  if (display_hour == 0) display_hour = 12;
  if (display_hour == s_active_hour) return false;
  s_active_hour = display_hour;
  layer_mark_dirty(s_face_layer);
  return true;
}