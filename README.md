# Percival

A clean, weather-focused watchface for Pebble smartwatches. Percival displays the current time, day, date, and battery level alongside three bottom weather complications showing the current temperature, daily high/low, and city where the weather has been requested from. Weather is fetched every 30 minutes from the Open-Meteo API, and city names are resolved via the BigDataCloud reverse geocoding API. Both are cached on-watch so data persists across app restarts. Primary color is configurable through the settings.

Percival is built with battery efficiency in mind. Weather and location requests are kept to a minimum — geolocation results are cached to avoid redundant GPS lookups, reverse geocoding is skipped when the user hasn't moved, and weather requests are only sent when the phone is connected via Bluetooth. Tested on Time 2 but should work on other devices too.
