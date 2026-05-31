#include "layer_weather.h"
#include "watchface.h"

// ============================================================================
// PRIVATE STATE
// ============================================================================

static Layer              *s_weather_layer;
static int                 s_temp_c    = 0;
static WeatherIconType     s_icon      = WeatherIconUnknown;
static bool                s_has_data  = false;
static bool                s_fahrenheit = false;
static GDrawCommandImage  *s_current_img = NULL;
static WeatherIconType     s_loaded_icon = (WeatherIconType)-1;
static char                s_temp_buf[8] = "--\xc2\xb0" "C";

static void format_temp_buf(void) {
  if (!s_has_data) {
    snprintf(s_temp_buf, sizeof(s_temp_buf), "--" "\xc2\xb0" "%c",
             s_fahrenheit ? 'F' : 'C');
    return;
  }
  int t = s_fahrenheit ? (s_temp_c * 9 / 5 + 32) : s_temp_c;
  snprintf(s_temp_buf, sizeof(s_temp_buf), "%d" "\xc2\xb0" "%c",
           t, s_fahrenheit ? 'F' : 'C');
}

static const uint32_t s_icon_resources[8] = {
  RESOURCE_ID_ICON_CLEAR,    // 0
  RESOURCE_ID_ICON_CLOUDY,   // 1
  RESOURCE_ID_ICON_FOG,      // 2
  RESOURCE_ID_ICON_DRIZZLE,  // 3
  RESOURCE_ID_ICON_RAIN,     // 4
  RESOURCE_ID_ICON_SNOW,     // 5
  RESOURCE_ID_ICON_STORM,    // 6
  RESOURCE_ID_ICON_UNKNOWN,  // 7
};

// Invert the RGB bits of a GColor8, preserving the alpha bits.
static GColor invert_gcolor(GColor c) {
  c.argb = (c.argb & 0xC0) | ((~c.argb) & 0x3F);
  return c;
}

static bool invert_command_colors(GDrawCommand *command, uint32_t index, void *context) {
  (void)index; (void)context;
  GColor fill   = gdraw_command_get_fill_color(command);
  GColor stroke = gdraw_command_get_stroke_color(command);
  gdraw_command_set_fill_color(command, invert_gcolor(fill));
  gdraw_command_set_stroke_color(command, invert_gcolor(stroke));
  return true;
}

static void invert_image_colors(GDrawCommandImage *img) {
  if (!img) return;
  GDrawCommandList *list = gdraw_command_image_get_command_list(img);
  if (!list) return;
  gdraw_command_list_iterate(list, invert_command_colors, NULL);
}

// Load only the currently-needed icon, releasing any previously loaded one.
static GDrawCommandImage* get_icon_image(WeatherIconType icon) {
  if (s_current_img && s_loaded_icon == icon) return s_current_img;
  if (s_current_img) {
    gdraw_command_image_destroy(s_current_img);
    s_current_img = NULL;
  }
  s_current_img = gdraw_command_image_create_with_resource(s_icon_resources[icon]);
  invert_image_colors(s_current_img);
  s_loaded_icon = icon;
  return s_current_img;
}

// ============================================================================
// LAYER UPDATE PROC
// ============================================================================

static void weather_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_antialiased(ctx, false);
  GRect bounds = layer_get_bounds(layer);
  int w  = bounds.size.w;
  int lh = bounds.size.h;

  const int GAP    = 4;
  const int TEXT_H = 32;
  const int MAX_TEXT_W = 72; // enough for "-99°C"

  GFont weather_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

  // Measure actual rendered text width so centering uses real content width
  GSize text_size = graphics_text_layout_get_content_size(
    s_temp_buf, weather_font,
    GRect(0, 0, MAX_TEXT_W, TEXT_H),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft
  );
  int text_w = text_size.w + 2; // +2 px safety margin

  // Resolve icon image and its size
  WeatherIconType icon = s_has_data ? s_icon : WeatherIconUnknown;
  if (icon > WeatherIconUnknown) icon = WeatherIconUnknown;
  GDrawCommandImage *img = get_icon_image(icon);
  GSize  icon_size = img ? gdraw_command_image_get_bounds_size(img) : GSize(0, 0);
  int    icon_w    = icon_size.w;
  int    icon_h    = icon_size.h;

  // Center the full widget (icon + gap + text) on the X axis
  int total_w = icon_w + (icon_w > 0 ? GAP : 0) + text_w;
  int start_x = (w - total_w) / 2;
  int cy      = lh / 2;

  // Draw icon
  if (img) {
    GPoint icon_origin = GPoint(start_x, cy - icon_h / 2);
    gdraw_command_image_draw(ctx, img, icon_origin);
  }

  // Draw temperature text
  graphics_context_set_text_color(ctx, GColorWhite);
  GRect text_rect = GRect(
    start_x + icon_w + (icon_w > 0 ? GAP : 0),
    cy - TEXT_H / 2,
    text_w,
    TEXT_H
  );
  graphics_draw_text(ctx, s_temp_buf, weather_font,
                     text_rect, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
}

// ============================================================================
// PUBLIC API
// ============================================================================

Layer* weather_layer_create(GRect bounds, Layer *parent) {
  // Icons are now loaded lazily on first draw; nothing to do here.

  // Place widget just below the 12 o'clock hour-number label, inside the
  // clock face interior — no overlap with ring, markers, or hour numbers.
  int face_h_edge  = s_h_radius - (CLOCK_FACE_STROKE_WIDTH / 2);
  int num_offset   = MAJOR_MARKER_LENGTH + s_num_offset;
  int label_bottom = s_center.y - (face_h_edge - num_offset) + 16;
  int layer_y      = label_bottom + 6;
  int layer_h      = 36;
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
  format_temp_buf();
  if (s_weather_layer) layer_mark_dirty(s_weather_layer);
}

void weather_layer_set_fahrenheit(bool fahrenheit) {
  if (s_fahrenheit == fahrenheit) return;
  s_fahrenheit = fahrenheit;
  format_temp_buf();
  if (s_weather_layer) layer_mark_dirty(s_weather_layer);
}

void weather_layer_destroy(void) {
  if (s_current_img) {
    gdraw_command_image_destroy(s_current_img);
    s_current_img = NULL;
  }
  if (s_weather_layer) {
    layer_destroy(s_weather_layer);
    s_weather_layer = NULL;
  }
}
