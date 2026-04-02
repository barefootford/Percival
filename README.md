# Percival

<p align="center">
  <img src="screenshots/percival-dark-blue.png" width="160" alt="Dark blue accent — SF">&nbsp;&nbsp;&nbsp;&nbsp;
  <img src="screenshots/percival-dark-red.png" width="160" alt="Dark red accent — NY">&nbsp;&nbsp;&nbsp;&nbsp;
  <img src="screenshots/percival-black.png" width="160" alt="Black accent">
</p>

A weather-focused Pebble watchface. Three location aware complications show:
- Current temperature
- Daily high/low
- Sunset time

Weather data is pulled from the Open-Meteo API every 30 minutes. Location is resolved through GPS coordinates via BigDataCloud. Cached when location changes less than 1km.

Color is configurable through settings.
