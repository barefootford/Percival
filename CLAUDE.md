# Percival

Pebble watchface built with Pebble SDK 3. Displays time, date, battery, and weather complications.

Percival was published April 7, 2026 in the Pebble store:
https://apps.repebble.com/2799cd581c2a4bbbade7f3da

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

## Marketing screenshots

Always use the `/marketing-screenshots` skill when generating or updating screenshots for README.md. It defines the full stubbing procedure (settings, weather, time, date, AppMessage disabling, emulator flags, and revert steps).

## Development workflow

After Claude implements a new feature, Claude always verifies the feature visually before considering it done:

1. `pebble clean && pebble build`
2. `pebble install --emulator emery`
3. `pebble screenshot --emulator emery` — capture a screenshot
4. Read the screenshot and evaluate whether the feature looks correct
5. Iterate if anything is broken or doesn't look right

Do not assume a feature works just because it compiles.

## Troubleshooting

### Emulator stuck at 100% CPU / `ConnectionResetError` on install

The SPI flash image occaisonally corrupts. The fix is to delete it so pebble-tool decompresses a fresh copy on next launch:

Replace `<platform>` with the emulator target (e.g. `emery`, `basalt`).
```
rm "$HOME/Library/Application Support/Pebble SDK/4.9.148/<platform>/qemu_spi_flash.bin"
```
