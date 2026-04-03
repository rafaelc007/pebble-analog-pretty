/**
 * PebbleKit JS — Weather fetcher for simple-watchface
 * API: Open-Meteo (free, no key required)
 * Sends: TEMPERATURE (int, Celsius) + WEATHER_ICON (int, 0-7)
 */

var xhrRequest = function(url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function() { callback(this.responseText); };
  xhr.open(type, url);
  xhr.send();
};

/**
 * Map WMO weather_code → icon int
 * 0=Clear  1=Cloudy  2=Fog  3=Drizzle  4=Rain  5=Snow  6=Storm  7=Unknown
 */
function weatherCodeToIcon(code) {
  if (code === 0)          return 0; // Clear
  if (code <= 3)           return 1; // Partly/Mostly cloudy
  if (code <= 48)          return 2; // Fog / depositing rime fog
  if (code <= 55)          return 3; // Drizzle
  if (code <= 57)          return 3; // Freezing drizzle → drizzle icon
  if (code <= 65)          return 4; // Rain
  if (code <= 67)          return 4; // Freezing rain → rain icon
  if (code <= 77)          return 5; // Snow / snow grains
  if (code <= 82)          return 4; // Rain showers
  if (code <= 86)          return 5; // Snow showers
  if (code === 95)         return 6; // Thunderstorm
  if (code <= 99)          return 6; // Thunderstorm with hail
  return 7;                          // Unknown
}

function locationSuccess(pos) {
  var url = 'https://api.open-meteo.com/v1/forecast?' +
    'latitude=' + pos.coords.latitude +
    '&longitude=' + pos.coords.longitude +
    '&current=temperature_2m,weather_code';

  xhrRequest(url, 'GET', function(responseText) {
    var json = JSON.parse(responseText);
    var temperature = Math.round(json.current.temperature_2m);
    var icon = weatherCodeToIcon(json.current.weather_code);

    Pebble.sendAppMessage(
      { 'TEMPERATURE': temperature, 'WEATHER_ICON': icon },
      function() { console.log('Weather sent to Pebble'); },
      function() { console.log('Error sending weather to Pebble'); }
    );
  });
}

function locationError(err) {
  console.log('Location error: ' + err.message);
}

function fetchWeather() {
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    { timeout: 15000, maximumAge: 300000 }
  );
}

Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready — fetching weather');
  fetchWeather();
});

Pebble.addEventListener('appmessage', function(e) {
  if (e.payload['dummy'] !== undefined) {
    fetchWeather();
  }
});
