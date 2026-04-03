#pragma once
#include <pebble.h>

typedef enum {
  WeatherIconClear   = 0,
  WeatherIconCloudy  = 1,
  WeatherIconFog     = 2,
  WeatherIconDrizzle = 3,
  WeatherIconRain    = 4,
  WeatherIconSnow    = 5,
  WeatherIconStorm   = 6,
  WeatherIconUnknown = 7,
} WeatherIconType;

// Creates the weather widget layer in the top quarter of the screen.
// Parent layer is the window root layer.
Layer* weather_layer_create(GRect bounds, Layer *parent);

// Updates the displayed temperature and icon. Call from inbox_received_callback.
void weather_layer_set_data(int temp_c, WeatherIconType icon);

// Destroys the weather layer — call from main_window_unload.
void weather_layer_destroy(void);
