var WEATHER_POLL_MINUTES = 30;

var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig);

var xhrRequest = function (url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.timeout = 15000;
  xhr.onload = function () {
    callback(xhr.status === 200 ? this.responseText : null);
  };
  xhr.onerror = function () { callback(null); };
  xhr.ontimeout = function () { callback(null); };
  xhr.open(type, url);
  xhr.send();
};

var cachedCity = null;
var lastLat = null;
var lastLon = null;
var LOCATION_THRESHOLD = 0.01; // ~1km movement before re-fetching city

var CITY_ABBREVIATIONS = {
  // Multi-word cities where initials are ambiguous or wrong
  'coeur dalene': 'CDA',   // API returns "Coeur d'Alene"
  'salt lake city': 'SLC', // API returns "Salt Lake City"
  'oklahoma city': 'OKC',  // API returns "Oklahoma City"
  'new orleans': 'NLA',    // API returns "New Orleans"
  'fort worth': 'FTW',     // API returns "Fort Worth"
  'fort collins': 'FTC',   // API returns "Fort Collins"
  // St. cities where "SL", "SP" etc. are confusing
  'st louis': 'STL',       // API returns "St. Louis" → normalizes to "st louis"
  'saint louis': 'STL',
  'saint paul': 'STP',     // API returns "Saint Paul"
  'st paul': 'STP',
  'st petersburg': 'SPB',  // API returns "St. Petersburg" → normalizes to "st petersburg"
  'saint petersburg': 'SPB',
  'st george': 'STG',
  'saint george': 'STG',
};

function getCityInitials(name) {
  var normalized = name.trim().toLowerCase().replace(/['.]/g, '');
  var lookup = CITY_ABBREVIATIONS[normalized];
  if (lookup) return lookup;
  var words = name.trim().split(/\s+/);
  if (words.length >= 2) {
    return (words[0].charAt(0) + words[1].charAt(0)).toUpperCase();
  }
  return words[0].substring(0, 3).toUpperCase();
}

function formatTo12h(isoString) {
  var parts = isoString.split('T')[1].split(':');
  var h = parseInt(parts[0], 10) % 12 || 12;
  return h + ':' + parts[1];
}

function locationMoved(lat, lon) {
  return lastLat === null ||
    Math.abs(lat - lastLat) > LOCATION_THRESHOLD ||
    Math.abs(lon - lastLon) > LOCATION_THRESHOLD;
}

function getTempUnit() {
  try {
    var settings = JSON.parse(localStorage.getItem('clay-settings'));
    if (settings && settings.TempUnit) {
      return parseInt(settings.TempUnit, 10) === 1 ? 'celsius' : 'fahrenheit';
    }
  } catch (e) {}
  return 'fahrenheit';
}

function locationSuccess(pos) {
  var lat = pos.coords.latitude;
  var lon = pos.coords.longitude;

  var unit = getTempUnit();
  var weatherUrl = 'https://api.open-meteo.com/v1/forecast?' +
    'latitude=' + lat + '&longitude=' + lon +
    '&current=temperature_2m,uv_index' +
    '&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset' +
    '&temperature_unit=' + unit +
    '&timezone=auto' +
    '&forecast_days=1';

  var needGeo = locationMoved(lat, lon);
  var weatherData = null;
  var cityInitials = cachedCity || '';
  var pending = needGeo ? 2 : 1;

  function trySend() {
    pending--;
    if (pending > 0) return;
    if (!weatherData) return;

    var set, rise;
    try {
      set = formatTo12h(weatherData.daily.sunset[0]);
      rise = formatTo12h(weatherData.daily.sunrise[0]);
    } catch (e) {
      console.log('Error parsing sun times: ' + e);
      return;
    }

    lastLat = lat;
    lastLon = lon;
    cachedCity = cityInitials;

    Pebble.sendAppMessage({
      'TEMPERATURE': Math.round(weatherData.current.temperature_2m),
      'TEMP_HIGH': Math.round(weatherData.daily.temperature_2m_max[0]),
      'TEMP_LOW': Math.round(weatherData.daily.temperature_2m_min[0]),
      'CITY': cityInitials,
      'SUNSET': set,
      'SUNRISE': rise,
      'UV_INDEX': weatherData.current.uv_index != null ? Math.round(weatherData.current.uv_index) : -1
    },
      function (e) { console.log('Weather sent successfully'); },
      function (e) { console.log('Error sending weather: ' + JSON.stringify(e)); }
    );
  }

  xhrRequest(weatherUrl, 'GET', function (weatherResp) {
    if (!weatherResp) {
      console.log('Weather request failed');
      trySend();
      return;
    }
    try { weatherData = JSON.parse(weatherResp); } catch (e) {
      console.log('Weather parse error: ' + e);
      trySend();
      return;
    }
    trySend();
  });

  if (needGeo) {
    var geoUrl = 'https://api.bigdatacloud.net/data/reverse-geocode-client?' +
      'latitude=' + lat + '&longitude=' + lon + '&localityLanguage=en';

    xhrRequest(geoUrl, 'GET', function (geoResp) {
      if (!geoResp) {
        console.log('Geocode request failed, using blank city');
        trySend();
        return;
      }
      try { var geo = JSON.parse(geoResp); } catch (e) {
        console.log('Geocode parse error: ' + e);
        trySend();
        return;
      }
      var city = geo.city || geo.locality || '';
      cityInitials = city ? getCityInitials(city) : '';
      trySend();
    });
  }
}

function locationError(err) {
  console.log('Error requesting location: ' + err);
}

function getWeather() {
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    { timeout: 15000, maximumAge: WEATHER_POLL_MINUTES * 60 * 1000 }
  );
}

Pebble.addEventListener('ready', function (e) {
  console.log('PebbleKit JS ready');
  getWeather();
});

Pebble.addEventListener('appmessage', function (e) {
  console.log('AppMessage received');
  getWeather();
});
