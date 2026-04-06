---
name: marketing-screenshots
description: Generate curated marketing screenshots for README.md by stubbing watchface data in the emulator
disable-model-invocation: false
---

Generate the curated marketing screenshots shown in README.md. Read SCREENSHOTS.md for the full stubbing reference, then follow the steps below.

## For each screenshot variant

Read the HTML comments in README.md above each `<img>` tag — they define the exact color, complication layout, weather values, date, and steps for that variant.

### 1. Stub settings in `default_settings()` (main.c)

Set `s_settings.primary_color` and all six complication slots to match the variant:
```c
static void default_settings() {
  s_settings.primary_color = GColorPictonBlue;  // or GColorBlack, GColorPurpureus
  s_settings.mini_comp_left = MINI_COMP_STEPS;
  s_settings.mini_comp_middle = MINI_COMP_SUNRISE;
  s_settings.mini_comp_right = MINI_COMP_BATTERY;
  s_settings.bottom_comp_left = BOTTOM_COMP_HIGHLOW;
  s_settings.bottom_comp_primary = BOTTOM_COMP_WEATHER;
  s_settings.bottom_comp_right = BOTTOM_COMP_SUNSET;
}
```

Comment out `persist_read_data(...)` in `load_settings()` so stale emulator data does not override the defaults:
```c
static void load_settings() {
  default_settings();
  // persist_read_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
}
```

### 2. Stub weather in `load_weather()` (main.c)

Replace the body with hardcoded values and set `s_weather.loaded = true`:
```c
static void load_weather() {
  memset(&s_weather, 0, sizeof(s_weather));
  snprintf(s_weather.temp, sizeof(s_weather.temp), "78");
  snprintf(s_weather.high, sizeof(s_weather.high), "82");
  snprintf(s_weather.low, sizeof(s_weather.low), "65");
  snprintf(s_weather.city, sizeof(s_weather.city), "LA");
  snprintf(s_weather.sunset, sizeof(s_weather.sunset), "7:32");
  snprintf(s_weather.sunrise, sizeof(s_weather.sunrise), "6:15");
  s_weather.loaded = true;
}
```

### 3. Disable AppMessage in `init()` (main.c)

Comment out all five `app_message_*` lines. If you skip this, the JS companion fetches real weather on install and overwrites the stubs:
```c
// app_message_register_inbox_received(inbox_received_callback);
// app_message_register_inbox_dropped(inbox_dropped_callback);
// app_message_register_outbox_failed(outbox_failed_callback);
// app_message_register_outbox_sent(outbox_sent_callback);
// app_message_open(256, 256);
```

### 4. Stub time in `update_time()` (main.c)

Replace the `text_layer_set_text` call with a hardcoded string. The emulator clock keeps ticking, so `emu-set-time` alone is unreliable:
```c
text_layer_set_text(s_time_layer, "10:00");
```

### 5. Stub date/steps/month/year in `update_status_buffer()` (main.c)

Hardcode whichever buffers the active complications need. The emulator cannot change the date and returns 0 for steps:
```c
snprintf(s_date_buffer, sizeof(s_date_buffer), "Sun 5");
snprintf(s_month_buffer, sizeof(s_month_buffer), "Apr");
snprintf(s_year_buffer, sizeof(s_year_buffer), "2026");
int steps = 23000; // instead of (int)health_service_sum_today(HealthMetricStepCount);
```

### 6. Build, install, and capture

```sh
pebble clean && pebble build
pebble install --emulator emery
pebble emu-battery --emulator emery --percent 100
pebble emu-bt-connection --emulator emery --connected yes
pebble screenshot --emulator emery
```

Move the resulting `pebble_screenshot_*.png` to `screenshots/` with the correct name.

### 7. Verify

Read the saved screenshot and compare it against the existing one. Flag any differences.

### 8. Repeat or revert

Apply the next variant's stubs and repeat from step 1. After capturing all variants, revert `main.c` with `git checkout src/c/main.c`.
