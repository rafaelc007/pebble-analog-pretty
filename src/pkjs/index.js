/**
 * PebbleKit JS — Weather fetcher + settings for simple-watchface
 * Weather API: Open-Meteo (free, no key required)
 * Sends: TEMPERATURE (int, Celsius) + WEATHER_ICON (int, 0-7)
 * Settings: SHAKE_SECONDS_ENABLED (bool as int 0/1)
 */

// ---------------------------------------------------------------------------
// Settings persistence (pkjs localStorage)
// ---------------------------------------------------------------------------
var SETTINGS_KEY = 'simple_watchface_settings';

function loadSettings() {
  try {
    var raw = localStorage.getItem(SETTINGS_KEY);
    if (raw) return JSON.parse(raw);
  } catch (e) {}
  return { shakeSecondsEnabled: true };
}

function saveSettings(s) {
  try { localStorage.setItem(SETTINGS_KEY, JSON.stringify(s)); } catch (e) {}
}

function sendSettings(s) {
  Pebble.sendAppMessage(
    { 'SHAKE_SECONDS_ENABLED': s.shakeSecondsEnabled ? 1 : 0 },
    function() { console.log('Settings sent to Pebble'); },
    function() { console.log('Error sending settings to Pebble'); }
  );
}

// ---------------------------------------------------------------------------
// Config page — self-contained data: URL, no hosting required
// ---------------------------------------------------------------------------
function buildConfigUrl(settings) {
  var checked = settings.shakeSecondsEnabled ? 'checked' : '';
  var html =
    '<!DOCTYPE html><html><head><meta charset="utf-8">' +
    '<meta name="viewport" content="width=device-width,initial-scale=1">' +
    '<title>simple-watchface</title>' +
    '<style>' +
    'body{font-family:-apple-system,Helvetica,Arial,sans-serif;background:#111;color:#eee;margin:0;padding:24px;}' +
    'h1{font-size:20px;margin:0 0 16px;}' +
    '.section{background:#1d1d1d;border-radius:8px;padding:16px;margin-bottom:16px;}' +
    '.row{display:flex;align-items:center;justify-content:space-between;}' +
    '.label{flex:1;}' +
    '.title{font-size:16px;font-weight:600;}' +
    '.desc{font-size:13px;color:#9a9a9a;margin-top:4px;}' +
    'input[type=checkbox]{transform:scale(1.6);margin-left:16px;accent-color:#00aaff;}' +
    'button{display:block;width:100%;padding:14px;border-radius:8px;border:0;font-size:16px;font-weight:600;background:#00aaff;color:#fff;}' +
    '</style></head><body>' +
    '<h1>Seconds Hand</h1>' +
    '<div class="section"><div class="row">' +
    '<div class="label">' +
    '<div class="title">Shake to show seconds</div>' +
    '<div class="desc">Tap the watch to reveal the seconds hand for 10 seconds. Turning this off disables the accelerometer for better battery life.</div>' +
    '</div>' +
    '<input id="shake" type="checkbox" ' + checked + '>' +
    '</div></div>' +
    '<button id="save">Save</button>' +
    '<script>' +
    'document.getElementById("save").addEventListener("click",function(){' +
    'var out={shakeSecondsEnabled:document.getElementById("shake").checked};' +
    'document.location="pebblejs://close#"+encodeURIComponent(JSON.stringify(out));' +
    '});' +
    '</script></body></html>';
  return 'data:text/html,' + encodeURIComponent(html);
}

// ---------------------------------------------------------------------------
// Weather
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Pebble lifecycle
// ---------------------------------------------------------------------------
Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready');
  // Push current settings to watch every launch so persisted prefs survive
  sendSettings(loadSettings());
  fetchWeather();
});

Pebble.addEventListener('appmessage', function(e) {
  if (e.payload['dummy'] !== undefined) {
    fetchWeather();
  }
});

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(buildConfigUrl(loadSettings()));
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e.response) return;
  var settings;
  try {
    settings = JSON.parse(decodeURIComponent(e.response));
  } catch (err) {
    console.log('Could not parse config response: ' + err);
    return;
  }
  saveSettings(settings);
  sendSettings(settings);
});
