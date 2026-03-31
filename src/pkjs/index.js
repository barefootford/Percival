var xhrRequest = function (url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    callback(this.responseText);
  };
  xhr.open(type, url);
  xhr.send();
};

function locationSuccess(pos) {
  var url = 'https://api.open-meteo.com/v1/forecast?' +
    'latitude=' + pos.coords.latitude +
    '&longitude=' + pos.coords.longitude +
    '&current=temperature_2m' +
    '&daily=temperature_2m_max,temperature_2m_min' +
    '&temperature_unit=fahrenheit' +
    '&forecast_days=1';

  xhrRequest(url, 'GET', function (responseText) {
    var json = JSON.parse(responseText);
    var temperature = Math.round(json.current.temperature_2m);
    var tempHigh = Math.round(json.daily.temperature_2m_max[0]);
    var tempLow = Math.round(json.daily.temperature_2m_min[0]);

    var dictionary = {
      'TEMPERATURE': temperature,
      'TEMP_HIGH': tempHigh,
      'TEMP_LOW': tempLow
    };

    Pebble.sendAppMessage(dictionary,
      function (e) { console.log('Weather sent successfully'); },
      function (e) { console.log('Error sending weather: ' + JSON.stringify(e)); }
    );
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
