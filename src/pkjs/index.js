var WEATHER_POLL_MINUTES = 30;

var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig);

var xhrRequest = function (url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    callback(this.responseText);
  };
  xhr.open(type, url);
  xhr.send();
};

var cachedCity = null;
var lastLat = null;
var lastLon = null;
var LOCATION_THRESHOLD = 0.01; // ~1km movement before re-fetching city

function getCityInitials(name) {
  var words = name.trim().split(/\s+/);
  if (words.length >= 2) {
    return (words[0].charAt(0) + words[1].charAt(0)).toUpperCase();
  }
  return words[0].charAt(0).toUpperCase();
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

function locationSuccess(pos) {
  var lat = pos.coords.latitude;
  var lon = pos.coords.longitude;

  var weatherUrl = 'https://api.open-meteo.com/v1/forecast?' +
    'latitude=' + lat + '&longitude=' + lon +
    '&current=temperature_2m' +
    '&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset' +
    '&temperature_unit=fahrenheit' +
    '&timezone=auto' +
    '&forecast_days=1';

  var needGeo = locationMoved(lat, lon);
  var weatherData = null;
  var cityInitials = cachedCity;
  var pending = needGeo ? 2 : 1;

  function trySend() {
    pending--;
    if (pending > 0) return;

    lastLat = lat;
    lastLon = lon;
    cachedCity = cityInitials;

    var set = formatTo12h(weatherData.daily.sunset[0]);
    var rise = formatTo12h(weatherData.daily.sunrise[0]);

    Pebble.sendAppMessage({
      'TEMPERATURE': Math.round(weatherData.current.temperature_2m),
      'TEMP_HIGH': Math.round(weatherData.daily.temperature_2m_max[0]),
      'TEMP_LOW': Math.round(weatherData.daily.temperature_2m_min[0]),
      'CITY': cityInitials,
      'SUNSET': set,
      'SUNRISE': rise
    },
      function (e) { console.log('Weather sent successfully'); },
      function (e) { console.log('Error sending weather: ' + JSON.stringify(e)); }
    );
  }

  xhrRequest(weatherUrl, 'GET', function (weatherResp) {
    weatherData = JSON.parse(weatherResp);
    trySend();
  });

  if (needGeo) {
    var geoUrl = 'https://api.bigdatacloud.net/data/reverse-geocode-client?' +
      'latitude=' + lat + '&longitude=' + lon + '&localityLanguage=en';

    xhrRequest(geoUrl, 'GET', function (geoResp) {
      var geo = JSON.parse(geoResp);
      var city = geo.city || geo.locality || '?';
      cityInitials = getCityInitials(city);
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
