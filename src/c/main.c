#include <pebble.h>

extern uint32_t MESSAGE_KEY_TEMPERATURE;
extern uint32_t MESSAGE_KEY_TEMP_HIGH;
extern uint32_t MESSAGE_KEY_TEMP_LOW;
extern uint32_t MESSAGE_KEY_CITY;
extern uint32_t MESSAGE_KEY_PrimaryColor;
extern uint32_t MESSAGE_KEY_SUNSET;
extern uint32_t MESSAGE_KEY_MiniCompLeft;
extern uint32_t MESSAGE_KEY_MiniCompMiddle;
extern uint32_t MESSAGE_KEY_MiniCompRight;
extern uint32_t MESSAGE_KEY_SUNRISE;
extern uint32_t MESSAGE_KEY_BottomCompLeft;
extern uint32_t MESSAGE_KEY_BottomCompPrimary;
extern uint32_t MESSAGE_KEY_BottomCompRight;
extern uint32_t MESSAGE_KEY_UV_INDEX;
extern uint32_t MESSAGE_KEY_Canvas;

enum MiniCompType {
  MINI_COMP_NONE = 0,
  MINI_COMP_DATE = 1,
  MINI_COMP_STEPS = 2,
  MINI_COMP_BATTERY = 3,
  MINI_COMP_YEAR = 4,
  MINI_COMP_SUNSET = 5,
  MINI_COMP_SUNRISE = 6,
  MINI_COMP_MONTH = 7,
  MINI_COMP_UV = 8,
  MINI_COMP_WEEK = 9
};

enum Canvas {
  CANVAS_PAPER = 0,
  CANVAS_INK = 1
};

enum BottomCompType {
  BOTTOM_COMP_NONE = 0,
  BOTTOM_COMP_HIGHLOW = 1,
  BOTTOM_COMP_WEATHER = 2,
  BOTTOM_COMP_SUNSET = 3,
  BOTTOM_COMP_SUNRISE = 4,
  BOTTOM_COMP_STEPS = 5,
  BOTTOM_COMP_WEEK = 6,
  BOTTOM_COMP_UV = 7
};

static Window *s_main_window;
static TextLayer *s_time_layer;
static Layer *s_brand_layer;
static Layer *s_complications_layer;
static Layer *s_top_bar_layer;
static Layer *s_window_layer;

static GBitmap *s_brand_bitmap;
static GBitmap *s_sneaker_bitmap;
static Layer *s_bt_layer;
static bool s_bt_app_connected;
static bool s_bt_radio_connected;
static Layer *s_qt_layer;

static GFont s_font_14;
static GFont s_font_16;
static GFont s_font_18;
static GFont s_font_28;
static GFont s_font_68;

#define TOP_BAR_HEIGHT 22
#define WEATHER_POLL_MINUTES 30
#define SETTINGS_KEY 1
#define WEATHER_KEY 2

// New fields must be appended to the end — never insert or reorder.
// load_settings() reads stored bytes and zero-fills the rest, so
// existing users keep their settings when the struct grows.
typedef struct {
  GColor primary_color;
  uint8_t mini_comp_left;
  uint8_t mini_comp_middle;
  uint8_t mini_comp_right;
  uint8_t bottom_comp_left;
  uint8_t bottom_comp_primary;
  uint8_t bottom_comp_right;
  uint8_t canvas;
} Settings;

static Settings s_settings;

static GColor fg_color() {
  return s_settings.canvas == CANVAS_INK ? GColorWhite : s_settings.primary_color;
}

static GColor bg_color() {
  return s_settings.canvas == CANVAS_INK ? s_settings.primary_color : GColorWhite;
}

static bool has_mini_comp(uint8_t type) {
  return (s_settings.mini_comp_left == type ||
          s_settings.mini_comp_middle == type ||
          s_settings.mini_comp_right == type);
}

static bool has_bottom_comp(uint8_t type) {
  return (s_settings.bottom_comp_left == type ||
          s_settings.bottom_comp_primary == type ||
          s_settings.bottom_comp_right == type);
}

static bool needs_weather() {
  return (has_mini_comp(MINI_COMP_SUNSET) ||
          has_mini_comp(MINI_COMP_SUNRISE) ||
          has_mini_comp(MINI_COMP_UV) ||
          has_bottom_comp(BOTTOM_COMP_HIGHLOW) ||
          has_bottom_comp(BOTTOM_COMP_WEATHER) ||
          has_bottom_comp(BOTTOM_COMP_SUNSET) ||
          has_bottom_comp(BOTTOM_COMP_SUNRISE) ||
          has_bottom_comp(BOTTOM_COMP_UV));
}

static bool needs_steps() {
  return (has_mini_comp(MINI_COMP_STEPS) ||
          has_bottom_comp(BOTTOM_COMP_STEPS));
}

static void default_settings() {
  s_settings.primary_color = GColorBlack;
  s_settings.mini_comp_left = MINI_COMP_DATE;
  s_settings.mini_comp_middle = MINI_COMP_STEPS;
  s_settings.mini_comp_right = MINI_COMP_BATTERY;
  s_settings.bottom_comp_left = BOTTOM_COMP_HIGHLOW;
  s_settings.bottom_comp_primary = BOTTOM_COMP_WEATHER;
  s_settings.bottom_comp_right = BOTTOM_COMP_SUNSET;
  s_settings.canvas = CANVAS_PAPER;
}

static void load_settings() {
  default_settings();
  if (persist_exists(SETTINGS_KEY)) {
    int stored = persist_get_size(SETTINGS_KEY);
    if (stored > 0 && (size_t)stored <= sizeof(s_settings)) {
      persist_read_data(SETTINGS_KEY, &s_settings, stored);
    }
  }
}

static void save_settings() {
  persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
}

typedef struct {
  char temp[8];
  char high[8];
  char low[8];
  char city[4];
  char sunset[8];
  char sunrise[8];
  char uv[12];
  bool loaded;
} WeatherCache;

static WeatherCache s_weather;

static void load_weather() {
  memset(&s_weather, 0, sizeof(s_weather));
  if (persist_exists(WEATHER_KEY)) {
    int stored = persist_get_size(WEATHER_KEY);
    if (stored > 0 && (size_t)stored <= sizeof(s_weather)) {
      persist_read_data(WEATHER_KEY, &s_weather, stored);
    }
  }
}

static void save_weather() {
  persist_write_data(WEATHER_KEY, &s_weather, sizeof(s_weather));
}

static int s_battery_level;
static char s_date_buffer[12];
static char s_battery_buffer[8];
static char s_steps_buffer[12];
static char s_year_buffer[12];
static char s_month_buffer[5];
static char s_sunset_mini_buffer[12];
static char s_sunrise_mini_buffer[12];
static char s_uv_buffer[16];
static char s_week_buffer[8];

static void update_status_buffer(struct tm *t);
static void update_display();

static void format_time_buffer(const char *src, char *dest, size_t size, const char *suffix) {
  if (s_weather.loaded && src[0]) {
    snprintf(dest, size, "%s%s", src, suffix);
  } else {
    snprintf(dest, size, "--");
  }
}

static void update_mini_weather_buffers() {
  format_time_buffer(s_weather.sunset, s_sunset_mini_buffer, sizeof(s_sunset_mini_buffer), "p");
  format_time_buffer(s_weather.sunrise, s_sunrise_mini_buffer, sizeof(s_sunrise_mini_buffer), "a");
  if (s_weather.loaded && s_weather.uv[0]) {
    snprintf(s_uv_buffer, sizeof(s_uv_buffer), "UV %s", s_weather.uv);
  } else {
    snprintf(s_uv_buffer, sizeof(s_uv_buffer), "UV -");
  }
}

static uint8_t tuple_to_uint8(Tuple *t) {
  return t->type == TUPLE_CSTRING ? (uint8_t)atoi(t->value->cstring) : (uint8_t)t->value->int32;
}


static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Weather data
  Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
  Tuple *high_tuple = dict_find(iterator, MESSAGE_KEY_TEMP_HIGH);
  Tuple *low_tuple = dict_find(iterator, MESSAGE_KEY_TEMP_LOW);

  if (temp_tuple) {
    snprintf(s_weather.temp, sizeof(s_weather.temp), "%d", (int)temp_tuple->value->int32);
  }
  if (high_tuple) {
    snprintf(s_weather.high, sizeof(s_weather.high), "%d", (int)high_tuple->value->int32);
  }
  if (low_tuple) {
    snprintf(s_weather.low, sizeof(s_weather.low), "%d", (int)low_tuple->value->int32);
  }

  Tuple *city_tuple = dict_find(iterator, MESSAGE_KEY_CITY);
  if (city_tuple) {
    snprintf(s_weather.city, sizeof(s_weather.city), "%s", city_tuple->value->cstring);
  }

  Tuple *sunset_tuple = dict_find(iterator, MESSAGE_KEY_SUNSET);
  if (sunset_tuple) {
    snprintf(s_weather.sunset, sizeof(s_weather.sunset), "%s", sunset_tuple->value->cstring);
  }

  Tuple *sunrise_tuple = dict_find(iterator, MESSAGE_KEY_SUNRISE);
  if (sunrise_tuple) {
    snprintf(s_weather.sunrise, sizeof(s_weather.sunrise), "%s", sunrise_tuple->value->cstring);
  }

  Tuple *uv_tuple = dict_find(iterator, MESSAGE_KEY_UV_INDEX);
  if (uv_tuple) {
    int uv_val = (int)uv_tuple->value->int32;
    if (uv_val >= 0) {
      snprintf(s_weather.uv, sizeof(s_weather.uv), "%d", uv_val);
    } else {
      s_weather.uv[0] = '\0';
    }
  }

  if (temp_tuple || high_tuple || low_tuple || city_tuple || sunset_tuple || sunrise_tuple || uv_tuple) {
    s_weather.loaded = true;
    save_weather();
    update_mini_weather_buffers();
    if (s_complications_layer) {
      layer_mark_dirty(s_complications_layer);
    }
    if (s_top_bar_layer) {
      layer_mark_dirty(s_top_bar_layer);
    }
  }

  // Settings
  Tuple *color_t = dict_find(iterator, MESSAGE_KEY_PrimaryColor);
  Tuple *left_t = dict_find(iterator, MESSAGE_KEY_MiniCompLeft);
  Tuple *mid_t = dict_find(iterator, MESSAGE_KEY_MiniCompMiddle);
  Tuple *right_t = dict_find(iterator, MESSAGE_KEY_MiniCompRight);
  Tuple *bleft_t = dict_find(iterator, MESSAGE_KEY_BottomCompLeft);
  Tuple *bpri_t = dict_find(iterator, MESSAGE_KEY_BottomCompPrimary);
  Tuple *bright_t = dict_find(iterator, MESSAGE_KEY_BottomCompRight);
  Tuple *canvas_t = dict_find(iterator, MESSAGE_KEY_Canvas);

  bool settings_changed = false;
  if (color_t) {
    s_settings.primary_color = GColorFromHEX(color_t->value->int32);
    settings_changed = true;
  }
  if (left_t) {
    s_settings.mini_comp_left = tuple_to_uint8(left_t);
    settings_changed = true;
  }
  if (mid_t) {
    s_settings.mini_comp_middle = tuple_to_uint8(mid_t);
    settings_changed = true;
  }
  if (right_t) {
    s_settings.mini_comp_right = tuple_to_uint8(right_t);
    settings_changed = true;
  }
  if (bleft_t) {
    s_settings.bottom_comp_left = tuple_to_uint8(bleft_t);
    settings_changed = true;
  }
  if (bpri_t) {
    s_settings.bottom_comp_primary = tuple_to_uint8(bpri_t);
    settings_changed = true;
  }
  if (bright_t) {
    s_settings.bottom_comp_right = tuple_to_uint8(bright_t);
    settings_changed = true;
  }
  if (canvas_t) {
    s_settings.canvas = tuple_to_uint8(canvas_t);
    settings_changed = true;
  }
  if (settings_changed) {
    save_settings();
    window_set_background_color(s_main_window, bg_color());
    update_display();
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static struct tm *get_time(struct tm *t) {
  if (t) return t;
  time_t now = time(NULL);
  return localtime(&now);
}

static void update_time(struct tm *tick_time) {
  struct tm *t = get_time(tick_time);

  static char s_time_buffer[8];
  strftime(s_time_buffer, sizeof(s_time_buffer), clock_is_24h_style() ?
                                                    "%H:%M" : "%I:%M", t);
  char *display = s_time_buffer;
  if (display[0] == '0') display++;
  text_layer_set_text(s_time_layer, display);
}

static void update_quiet_time();

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time(tick_time);
  update_status_buffer(tick_time);
  update_quiet_time();

  // Request weather update every 30 minutes (only if needed and phone is connected)
  if (tick_time->tm_min % WEATHER_POLL_MINUTES == 0 &&
      needs_weather() &&
      connection_service_peek_pebble_app_connection()) {
    DictionaryIterator *iter;
    if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
      dict_write_uint8(iter, 0, 0);
      app_message_outbox_send();
    }
  }
}

static void update_status_buffer(struct tm *tick_time) {
  struct tm *t = get_time(tick_time);
  static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const char *day = days[t->tm_wday];
  static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "June",
                                   "July", "Aug", "Sep", "Oct", "Nov", "Dec"};
  snprintf(s_year_buffer, sizeof(s_year_buffer), "%d", t->tm_year + 1900);
  snprintf(s_month_buffer, sizeof(s_month_buffer), "%s", months[t->tm_mon]);
  strftime(s_week_buffer, sizeof(s_week_buffer), "W%V", t);
  snprintf(s_date_buffer, sizeof(s_date_buffer), "%s %d", day, t->tm_mday);
  snprintf(s_battery_buffer, sizeof(s_battery_buffer), "%d%%", s_battery_level);

  if (needs_steps()) {
    int steps = (int)health_service_sum_today(HealthMetricStepCount);
    if (steps >= 1000) {
      snprintf(s_steps_buffer, sizeof(s_steps_buffer), "%dk", steps / 1000);
    } else {
      snprintf(s_steps_buffer, sizeof(s_steps_buffer), "%d", steps);
    }
  }

  if (s_top_bar_layer) {
    layer_mark_dirty(s_top_bar_layer);
  }
}

static void battery_callback(BatteryChargeState state) {
  s_battery_level = state.charge_percent;
  update_status_buffer(NULL);
}

static void update_bt_visibility() {
  if (s_bt_layer) {
    layer_set_hidden(s_bt_layer, s_bt_app_connected && s_bt_radio_connected);
  }
}

static void bt_app_callback(bool connected) {
  s_bt_app_connected = connected;
  update_bt_visibility();
}

static void bt_radio_callback(bool connected) {
  s_bt_radio_connected = connected;
  update_bt_visibility();
}

static void bt_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_text_color(ctx, GColorRed);
  graphics_draw_text(ctx, "BT", s_font_18, bounds,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}

static void qt_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_text_color(ctx, fg_color());
  graphics_draw_text(ctx, "QT", s_font_18, bounds,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void update_quiet_time() {
  if (s_qt_layer) {
    layer_set_hidden(s_qt_layer, !quiet_time_is_active());
  }
}

static void update_display() {
  if (s_time_layer) {
    text_layer_set_text_color(s_time_layer, fg_color());
  }
  if (s_brand_layer) {
    layer_mark_dirty(s_brand_layer);
  }
  if (s_top_bar_layer) {
    layer_mark_dirty(s_top_bar_layer);
  }
  if (s_complications_layer) {
    layer_mark_dirty(s_complications_layer);
  }
  if (s_qt_layer) {
    layer_mark_dirty(s_qt_layer);
  }
}

static void brand_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GSize bmp_size = gbitmap_get_bounds(s_brand_bitmap).size;
  int x = (bounds.size.w - bmp_size.w) / 2;
  int y = (bounds.size.h - bmp_size.h) / 2;

  #ifdef PBL_COLOR
    static GColor palette[2];
    palette[0] = fg_color();
    palette[1] = GColorClear;
    gbitmap_set_palette(s_brand_bitmap, palette, false);
  #endif
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, s_brand_bitmap, GRect(x, y, bmp_size.w, bmp_size.h));
}

static const char* get_mini_comp_text(uint8_t type) {
  switch (type) {
    case MINI_COMP_DATE: return s_date_buffer;
    case MINI_COMP_STEPS: return s_steps_buffer;
    case MINI_COMP_BATTERY: return s_battery_buffer;
    case MINI_COMP_YEAR: return s_year_buffer;
    case MINI_COMP_SUNSET: return s_sunset_mini_buffer;
    case MINI_COMP_SUNRISE: return s_sunrise_mini_buffer;
    case MINI_COMP_MONTH: return s_month_buffer;
    case MINI_COMP_UV: return s_uv_buffer;
    case MINI_COMP_WEEK: return s_week_buffer;
    default: return NULL;
  }
}

static void top_bar_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  bool ink = s_settings.canvas == CANVAS_INK;

  if (!ink) {
    graphics_context_set_fill_color(ctx, s_settings.primary_color);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  }

  const char *items_in[3] = {
    get_mini_comp_text(s_settings.mini_comp_left),
    get_mini_comp_text(s_settings.mini_comp_middle),
    get_mini_comp_text(s_settings.mini_comp_right),
  };

  graphics_context_set_text_color(ctx, GColorWhite);
  int pad = 4;
  int dot_r = 2;
  int dot_d = 2 * dot_r;
  int dot_y = bounds.size.h / 2;
  GRect probe_rect = GRect(0, 0, bounds.size.w, bounds.size.h);

  const char *active[3];
  GSize sizes[3];
  int n = 0;
  int total_text_w = 0;
  for (int i = 0; i < 3; i++) {
    if (items_in[i]) {
      active[n] = items_in[i];
      sizes[n] = graphics_text_layout_get_content_size(
          items_in[i], s_font_16, probe_rect,
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
      total_text_w += sizes[n].w;
      n++;
    }
  }

  if (n > 0) {
    int avail = bounds.size.w - 2 * pad - total_text_w - (n - 1) * dot_d;
    int gap = (n > 1) ? avail / (2 * (n - 1)) : 0;
    int x = pad + (n == 1 ? avail / 2 : 0);

    graphics_context_set_fill_color(ctx, GColorWhite);
    for (int i = 0; i < n; i++) {
      GRect r = GRect(x, 2, sizes[i].w, bounds.size.h);
      graphics_draw_text(ctx, active[i], s_font_16, r,
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
      x += sizes[i].w;
      if (i < n - 1) {
        x += gap;
        graphics_fill_circle(ctx, GPoint(x + dot_r, dot_y), dot_r);
        x += dot_d + gap;
      }
    }
  }

  if (ink) {
    graphics_fill_rect(ctx, GRect(0, bounds.size.h - 1, bounds.size.w, 1), 0, GCornerNone);
  }
}

static void draw_comp_highlow(GContext *ctx, int cx, int cy, int radius, GColor fg) {
  graphics_context_set_text_color(ctx, fg);
  GRect top_rect = GRect(cx - radius, cy - radius + 7, radius * 2, radius - 7);
  graphics_draw_text(ctx, s_weather.loaded ? s_weather.high : "--", s_font_18,
                     top_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  int line_margin = 10;
  graphics_context_set_stroke_color(ctx, fg);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(cx - radius + line_margin, cy),
                         GPoint(cx + radius - line_margin, cy));

  GRect bot_rect = GRect(cx - radius, cy + 2, radius * 2, radius - 2);
  graphics_draw_text(ctx, s_weather.loaded ? s_weather.low : "--", s_font_18,
                     bot_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void draw_comp_weather(GContext *ctx, int cx, int cy, int radius, GColor fg) {
  const char *city = s_weather.loaded ? s_weather.city : "--";
  const char *temp = s_weather.loaded ? s_weather.temp : "--";

  graphics_context_set_text_color(ctx, fg);
  GRect city_rect = GRect(cx - radius, cy - radius + 7, radius * 2, 18);
  graphics_draw_text(ctx, city, s_font_14, city_rect,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  GRect temp_rect = GRect(cx - radius, cy - 8, radius * 2, 34);
  graphics_draw_text(ctx, temp, s_font_28, temp_rect,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  if (s_weather.loaded) {
    GSize ts = graphics_text_layout_get_content_size(
        temp, s_font_28, temp_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
    graphics_context_set_fill_color(ctx, fg);
    graphics_fill_circle(ctx, GPoint(cx + ts.w / 2 + 3, temp_rect.origin.y + 7), 2);
  }
}

static void draw_comp_week(GContext *ctx, int cx, int cy, int radius, GColor fg) {
  graphics_context_set_text_color(ctx, fg);
  GRect label_rect = GRect(cx - radius, cy - radius + 7, radius * 2, 18);
  graphics_draw_text(ctx, "WK", s_font_14, label_rect,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  GRect num_rect = GRect(cx - radius, cy - 8, radius * 2, 34);
  graphics_draw_text(ctx, &s_week_buffer[1], s_font_28, num_rect,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void draw_comp_uv(GContext *ctx, int cx, int cy, int radius, GColor fg) {
  const char *uv_num = (s_weather.loaded && s_weather.uv[0]) ? s_weather.uv : "-";

  graphics_context_set_text_color(ctx, fg);
  GRect label_rect = GRect(cx - radius, cy - radius + 7, radius * 2, 18);
  graphics_draw_text(ctx, "UV", s_font_14, label_rect,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  GRect num_rect = GRect(cx - radius, cy - 8, radius * 2, 34);
  graphics_draw_text(ctx, uv_num, s_font_28, num_rect,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void draw_comp_sun(GContext *ctx, int cx, int cy, int radius,
                          GColor fg, GColor bg, const char *time_str, bool fill_sun) {
  int sun_r = 12;
  int horizon_y = cy - 2;

  if (fill_sun) {
    graphics_context_set_fill_color(ctx, fg);
    graphics_fill_circle(ctx, GPoint(cx, horizon_y), sun_r);
    graphics_context_set_fill_color(ctx, bg);
    graphics_fill_rect(ctx, GRect(cx - sun_r - 1, horizon_y, sun_r * 2 + 2, sun_r + 2), 0, GCornerNone);
  } else {
    graphics_context_set_stroke_color(ctx, fg);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_circle(ctx, GPoint(cx, horizon_y), sun_r);
    // Mask bottom half of the stroked circle
    graphics_context_set_fill_color(ctx, bg);
    graphics_fill_rect(ctx, GRect(cx - sun_r - 1, horizon_y, sun_r * 2 + 2, sun_r + 2), 0, GCornerNone);
  }

  graphics_context_set_stroke_color(ctx, fg);
  graphics_context_set_stroke_width(ctx, 2);
  int hz_w = sun_r + 8;
  graphics_draw_line(ctx, GPoint(cx - hz_w, horizon_y), GPoint(cx + hz_w, horizon_y));

  int ray_start = sun_r + 3;
  int ray_end = sun_r + 7;
  graphics_draw_line(ctx, GPoint(cx, horizon_y - ray_start),
                         GPoint(cx, horizon_y - ray_end));
  int ds = ray_start * 7 / 10;
  int de = ray_end * 7 / 10;
  graphics_draw_line(ctx, GPoint(cx - ds, horizon_y - ds),
                         GPoint(cx - de, horizon_y - de));
  graphics_draw_line(ctx, GPoint(cx + ds, horizon_y - ds),
                         GPoint(cx + de, horizon_y - de));

  graphics_context_set_text_color(ctx, fg);
  GRect time_rect = GRect(cx - radius, cy, radius * 2, 24);
  graphics_draw_text(ctx, time_str, s_font_18, time_rect,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void draw_comp_steps(GContext *ctx, int cx, int cy, int radius, GColor fg) {
  GSize bmp_size = gbitmap_get_bounds(s_sneaker_bitmap).size;
  int icon_x = cx - bmp_size.w / 2;
  int icon_y = cy + 4 - bmp_size.h;

  #ifdef PBL_COLOR
    static GColor palette[2];
    palette[0] = fg;
    palette[1] = GColorClear;
    gbitmap_set_palette(s_sneaker_bitmap, palette, false);
  #endif
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, s_sneaker_bitmap, GRect(icon_x, icon_y, bmp_size.w, bmp_size.h));

  graphics_context_set_text_color(ctx, fg);
  GRect text_rect = GRect(cx - radius, cy, radius * 2, 24);
  graphics_draw_text(ctx, s_steps_buffer, s_font_18, text_rect,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void draw_bottom_comp(GContext *ctx, uint8_t type, int cx, int cy, int radius, bool is_primary) {
  if (type == BOTTOM_COMP_NONE) return;

  GColor fg, bg;
  if (is_primary) {
    fg = bg_color();
    bg = fg_color();
    graphics_context_set_fill_color(ctx, bg);
    graphics_fill_circle(ctx, GPoint(cx, cy), radius);
  } else {
    fg = fg_color();
    bg = bg_color();
    graphics_context_set_stroke_color(ctx, fg);
    graphics_context_set_stroke_width(ctx, s_settings.canvas == CANVAS_INK ? 1 : 2);
    graphics_draw_circle(ctx, GPoint(cx, cy), radius);
  }

  switch (type) {
    case BOTTOM_COMP_HIGHLOW:
      draw_comp_highlow(ctx, cx, cy, radius, fg);
      break;
    case BOTTOM_COMP_WEATHER:
      draw_comp_weather(ctx, cx, cy, radius, fg);
      break;
    case BOTTOM_COMP_SUNSET: {
      const char *set = (s_weather.loaded && s_weather.sunset[0]) ? s_weather.sunset : "--";
      draw_comp_sun(ctx, cx, cy, radius, fg, bg, set, true);
      break;
    }
    case BOTTOM_COMP_SUNRISE: {
      const char *rise = (s_weather.loaded && s_weather.sunrise[0]) ? s_weather.sunrise : "--";
      draw_comp_sun(ctx, cx, cy, radius, fg, bg, rise, false);
      break;
    }
    case BOTTOM_COMP_STEPS:
      draw_comp_steps(ctx, cx, cy, radius, fg);
      break;
    case BOTTOM_COMP_WEEK:
      draw_comp_week(ctx, cx, cy, radius, fg);
      break;
    case BOTTOM_COMP_UV:
      draw_comp_uv(ctx, cx, cy, radius, fg);
      break;
  }
}

static void complications_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  int circle_radius = 30;
  int y_center = bounds.size.h / 2;

  int section_w = bounds.size.w / 3;
  int cx1 = section_w / 2;
  int cx2 = section_w + section_w / 2;
  int cx3 = 2 * section_w + section_w / 2;

  draw_bottom_comp(ctx, s_settings.bottom_comp_left, cx1, y_center, circle_radius, false);
  draw_bottom_comp(ctx, s_settings.bottom_comp_primary, cx2, y_center, circle_radius, true);
  draw_bottom_comp(ctx, s_settings.bottom_comp_right, cx3, y_center, circle_radius, false);
}

static void update_layout() {
  GRect full = layer_get_bounds(s_window_layer);
  GRect unob = layer_get_unobstructed_bounds(s_window_layer);
  int obstructed = full.size.h - unob.size.h;

  layer_set_hidden(s_complications_layer, obstructed > 0);

  int comp_space = 70;
  int bottom = unob.size.h < (full.size.h - comp_space)
             ? unob.size.h
             : (full.size.h - comp_space);
  int group_h = 76 + 18;
  int group_y = TOP_BAR_HEIGHT + (bottom - TOP_BAR_HEIGHT - group_h) / 2;

  layer_set_frame(text_layer_get_layer(s_time_layer),
                  GRect(0, group_y, full.size.w, 76));
  layer_set_frame(s_bt_layer,
                  GRect(full.size.w - 54, TOP_BAR_HEIGHT + 2, 50, 22));
  layer_set_frame(s_qt_layer,
                  GRect(4, TOP_BAR_HEIGHT + 2, 50, 22));
  layer_set_frame(s_brand_layer,
                  GRect(0, group_y + 76, full.size.w, 20));
}

static void unobstructed_change(AnimationProgress progress, void *context) {
  update_layout();
}

static void unobstructed_did_change(void *context) {
  update_layout();
}

static void main_window_load(Window *window) {
  s_window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(s_window_layer);

  // Load resources
  s_brand_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PEBBLE_LOGO);
  s_sneaker_bitmap = gbitmap_create_with_resource(RESOURCE_ID_SNEAKER_ICON);
  s_font_14 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_14));
  s_font_16 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_16));
  s_font_18 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_18));
  s_font_28 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_28));
  s_font_68 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_68));

  // Top bar
  s_top_bar_layer = layer_create(GRect(0, 0, bounds.size.w, TOP_BAR_HEIGHT));
  layer_set_update_proc(s_top_bar_layer, top_bar_update_proc);

  // Time layer (positioned by update_layout)
  s_time_layer = text_layer_create(GRect(0, 0, bounds.size.w, 76));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, fg_color());
  text_layer_set_font(s_time_layer, s_font_68);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  // Brand text (positioned by update_layout)
  s_brand_layer = layer_create(GRect(0, 0, bounds.size.w, 20));
  layer_set_update_proc(s_brand_layer, brand_update_proc);

  // Bluetooth disconnect indicator (positioned by update_layout)
  s_bt_layer = layer_create(GRect(0, 0, 50, 22));
  layer_set_update_proc(s_bt_layer, bt_update_proc);
  layer_set_hidden(s_bt_layer, true);

  // Quiet time indicator (positioned by update_layout)
  s_qt_layer = layer_create(GRect(0, 0, 50, 22));
  layer_set_update_proc(s_qt_layer, qt_update_proc);
  layer_set_hidden(s_qt_layer, true);

  // Complications layer at the bottom
  int comp_height = 66;
  s_complications_layer = layer_create(GRect(0, bounds.size.h - comp_height - 4, bounds.size.w, comp_height));
  layer_set_update_proc(s_complications_layer, complications_update_proc);

  layer_add_child(s_window_layer, s_brand_layer);
  layer_add_child(s_window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(s_window_layer, s_bt_layer);
  layer_add_child(s_window_layer, s_qt_layer);
  layer_add_child(s_window_layer, s_complications_layer);
  layer_add_child(s_window_layer, s_top_bar_layer);

  UnobstructedAreaHandlers ua_handlers = {
    .change = unobstructed_change,
    .did_change = unobstructed_did_change
  };
  unobstructed_area_service_subscribe(ua_handlers, NULL);
  update_layout();
}

static void main_window_unload(Window *window) {
  unobstructed_area_service_unsubscribe();
  layer_destroy(s_brand_layer);
  text_layer_destroy(s_time_layer);
  layer_destroy(s_complications_layer);
  layer_destroy(s_top_bar_layer);
  layer_destroy(s_bt_layer);
  layer_destroy(s_qt_layer);
  gbitmap_destroy(s_brand_bitmap);
  gbitmap_destroy(s_sneaker_bitmap);
  fonts_unload_custom_font(s_font_14);
  fonts_unload_custom_font(s_font_16);
  fonts_unload_custom_font(s_font_18);
  fonts_unload_custom_font(s_font_28);
  fonts_unload_custom_font(s_font_68);
}

static void init() {
  load_settings();
  load_weather();
  update_mini_weather_buffers();
  s_main_window = window_create();
  window_set_background_color(s_main_window, bg_color());
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);

  update_time(NULL);
  update_status_buffer(NULL);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  battery_state_service_subscribe(battery_callback);
  battery_callback(battery_state_service_peek());

  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bt_app_callback
  });
  s_bt_app_connected = connection_service_peek_pebble_app_connection();
  bluetooth_connection_service_subscribe(bt_radio_callback);
  s_bt_radio_connected = bluetooth_connection_service_peek();
  update_bt_visibility();
  update_quiet_time();

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  app_message_open(256, 256);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
