# Percival

Pebble watchface built with Pebble SDK 3. Displays time, date, battery, and weather complications.

## Build

```
pebble build
pebble install --emulator emery   # Time 2
pebble install --phone <IP>
```

Run `pebble clean` when adding or removing messageKeys in package.json — the build uses generated code that becomes stale otherwise.

## Structure

- `src/c/main.c` — watchface C code (UI, tick handler, persistent storage, weather message handling)
- `src/pkjs/index.js` — companion JS (geolocation, Open-Meteo weather API, BigDataCloud reverse geocoding)
- `src/pkjs/config.js` — Clay settings page config
- `package.json` — app metadata, message keys, font resources

## Key conventions

- Weather polling interval is defined as `WEATHER_POLL_MINUTES` in both `main.c` and `index.js` — keep them in sync
- Persistent storage keys: `SETTINGS_KEY = 1` (accent color), `WEATHER_KEY = 2` (cached weather data)
- Temperature unit is hardcoded to Fahrenheit in the Open-Meteo API call

## Troubleshooting

### Emulator stuck at 100% CPU / `ConnectionResetError` on install

The SPI flash image occaisonally corrupts. The fix is to delete it so pebble-tool decompresses a fresh copy on next launch:

Replace `<platform>` with the emulator target (e.g. `emery`, `basalt`).
```
rm "$HOME/Library/Application Support/Pebble SDK/4.9.148/<platform>/qemu_spi_flash.bin"
```
