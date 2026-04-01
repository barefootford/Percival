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

function getCityInitials(name) {
  var words = name.trim().split(/\s+/);
  if (words.length >= 2) {
    return (words[0].charAt(0) + words[1].charAt(0)).toUpperCase();
  }
  return words[0].charAt(0).toUpperCase();
}

function locationSuccess(pos) {
  var lat = pos.coords.latitude;
  var lon = pos.coords.longitude;

  var weatherUrl = 'https://api.open-meteo.com/v1/forecast?' +
    'latitude=' + lat + '&longitude=' + lon +
    '&current=temperature_2m' +
    '&daily=temperature_2m_max,temperature_2m_min' +
    '&temperature_unit=fahrenheit' +
    '&forecast_days=1';

  var geoUrl = 'https://api.bigdatacloud.net/data/reverse-geocode-client?' +
    'latitude=' + lat + '&longitude=' + lon + '&localityLanguage=en';

  xhrRequest(weatherUrl, 'GET', function (weatherResp) {
    var weather = JSON.parse(weatherResp);

    xhrRequest(geoUrl, 'GET', function (geoResp) {
      var geo = JSON.parse(geoResp);
      var city = geo.city || geo.locality || '?';

      Pebble.sendAppMessage({
        'TEMPERATURE': Math.round(weather.current.temperature_2m),
        'TEMP_HIGH': Math.round(weather.daily.temperature_2m_max[0]),
        'TEMP_LOW': Math.round(weather.daily.temperature_2m_min[0]),
        'CITY': getCityInitials(city)
      },
        function (e) { console.log('Weather sent successfully'); },
        function (e) { console.log('Error sending weather: ' + JSON.stringify(e)); }
      );
    });
  });
}

function locationError(err) {
  console.log('Error requesting location: ' + err);
}

function getWeather() {
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    { timeout: 15000, maximumAge: 60000 }
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
