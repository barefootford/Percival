#include <pebble.h>

extern uint32_t MESSAGE_KEY_TEMPERATURE;
extern uint32_t MESSAGE_KEY_TEMP_HIGH;
extern uint32_t MESSAGE_KEY_TEMP_LOW;
extern uint32_t MESSAGE_KEY_CITY;
extern uint32_t MESSAGE_KEY_PrimaryColor;
extern uint32_t MESSAGE_KEY_SUNSET;

static Window *s_main_window;
static TextLayer *s_time_layer;
static Layer *s_brand_layer;
static Layer *s_complications_layer;
static Layer *s_top_bar_layer;
static Layer *s_window_layer;

static GBitmap *s_brand_bitmap;

static GFont s_font_14;
static GFont s_font_18;
static GFont s_font_28;
static GFont s_font_68;

#define TOP_BAR_HEIGHT 20
#define WEATHER_POLL_MINUTES 30
#define SETTINGS_KEY 1
#define WEATHER_KEY 2

typedef struct {
  GColor primary_color;
} Settings;

static Settings s_settings;

static void prv_default_settings() {
  s_settings.primary_color = GColorBlack;
}

static void prv_load_settings() {
  prv_default_settings();
  persist_read_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
}

static void prv_save_settings() {
  persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
}

typedef struct {
  char temp[8];
  char high[8];
  char low[8];
  char city[4];
  char sunset[8];
  bool loaded;
} WeatherCache;

static WeatherCache s_weather;

static void prv_load_weather() {
  memset(&s_weather, 0, sizeof(s_weather));
  if (persist_exists(WEATHER_KEY)) {
    persist_read_data(WEATHER_KEY, &s_weather, sizeof(s_weather));
  }
}

static void prv_save_weather() {
  persist_write_data(WEATHER_KEY, &s_weather, sizeof(s_weather));
}

static int s_battery_level;
static char s_status_buffer[24];

static void update_status_buffer();
static void prv_update_display();


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

  if (temp_tuple || high_tuple || low_tuple || city_tuple || sunset_tuple) {
    s_weather.loaded = true;
    prv_save_weather();
    if (s_complications_layer) {
      layer_mark_dirty(s_complications_layer);
    }
  }

  // Settings
  Tuple *accent_t = dict_find(iterator, MESSAGE_KEY_PrimaryColor);
  if (accent_t) {
    s_settings.primary_color = GColorFromHEX(accent_t->value->int32);
    prv_save_settings();
    prv_update_display();
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

static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  static char s_time_buffer[8];
  strftime(s_time_buffer, sizeof(s_time_buffer), clock_is_24h_style() ?
                                                    "%H:%M" : "%I:%M", tick_time);
  char *display = s_time_buffer;
  if (display[0] == '0') display++;
  text_layer_set_text(s_time_layer, display);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  update_status_buffer();

  // Request weather update every 30 minutes (only if phone is connected)
  if (tick_time->tm_min % WEATHER_POLL_MINUTES == 0 &&
      connection_service_peek_pebble_app_connection()) {
    DictionaryIterator *iter;
    if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
      dict_write_uint8(iter, 0, 0);
      app_message_outbox_send();
    }
  }
}

static void update_status_buffer() {
  time_t temp = time(NULL);
  struct tm *t = localtime(&temp);
  static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  snprintf(s_status_buffer, sizeof(s_status_buffer), "%s %d | %d%%",
           days[t->tm_wday], t->tm_mday, s_battery_level);
  if (s_top_bar_layer) {
    layer_mark_dirty(s_top_bar_layer);
  }
}

static void battery_callback(BatteryChargeState state) {
  s_battery_level = state.charge_percent;
  update_status_buffer();
}

static void prv_update_display() {
  if (s_time_layer) {
    text_layer_set_text_color(s_time_layer, s_settings.primary_color);
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
}

static void brand_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GSize bmp_size = gbitmap_get_bounds(s_brand_bitmap).size;
  int x = (bounds.size.w - bmp_size.w) / 2;
  int y = (bounds.size.h - bmp_size.h) / 2;

  #ifdef PBL_COLOR
    GColor palette[] = {s_settings.primary_color, GColorClear};
    gbitmap_set_palette(s_brand_bitmap, palette, false);
  #endif
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, s_brand_bitmap, GRect(x, y, bmp_size.w, bmp_size.h));
}

static void top_bar_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, s_settings.primary_color);
  graphics_fill_rect(ctx, bounds, 8, GCornersBottom);

  graphics_context_set_text_color(ctx, GColorWhite);
  GRect text_rect = GRect(4, 2, bounds.size.w - 8, bounds.size.h);
  graphics_draw_text(ctx, s_status_buffer, s_font_14,
                     text_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}

static void complications_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  int circle_radius = 30;
  int y_center = bounds.size.h / 2;

  // Three complications evenly spaced
  int section_w = bounds.size.w / 3;
  int cx1 = section_w / 2;
  int cx2 = section_w + section_w / 2;
  int cx3 = 2 * section_w + section_w / 2;

  // === Left: High/Low stroked circle ===
  graphics_context_set_stroke_color(ctx, s_settings.primary_color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, GPoint(cx1, y_center), circle_radius);

  graphics_context_set_text_color(ctx, s_settings.primary_color);
  GRect top_rect = GRect(cx1 - circle_radius, y_center - circle_radius + 7,
                          circle_radius * 2, circle_radius - 7);
  graphics_draw_text(ctx, s_weather.loaded ? s_weather.high : "--", s_font_18,
                     top_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  int line_margin = 10;
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(cx1 - circle_radius + line_margin, y_center),
                         GPoint(cx1 + circle_radius - line_margin, y_center));

  GRect bot_rect = GRect(cx1 - circle_radius, y_center + 2,
                          circle_radius * 2, circle_radius - 2);
  graphics_draw_text(ctx, s_weather.loaded ? s_weather.low : "--", s_font_18,
                     bot_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // === Center: Filled circle, city + current temp ===
  const char *city = s_weather.loaded ? s_weather.city : "--";
  const char *temp = s_weather.loaded ? s_weather.temp : "--";

  graphics_context_set_fill_color(ctx, s_settings.primary_color);
  graphics_fill_circle(ctx, GPoint(cx2, y_center), circle_radius);

  graphics_context_set_text_color(ctx, GColorWhite);
  GRect city_rect = GRect(cx2 - circle_radius, y_center - circle_radius + 7,
                            circle_radius * 2, 18);
  graphics_draw_text(ctx, city, s_font_14, city_rect,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  GRect temp_rect = GRect(cx2 - circle_radius, y_center - 8,
                            circle_radius * 2, 34);
  graphics_draw_text(ctx, temp, s_font_28, temp_rect,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  if (s_weather.loaded) {
    GSize ts = graphics_text_layout_get_content_size(
        temp, s_font_28, temp_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_circle(ctx, GPoint(cx2 + ts.w / 2 + 3, temp_rect.origin.y + 7), 2);
  }

  // === Right: Sunset stroked circle with glyph ===
  const char *set = (s_weather.loaded && s_weather.sunset[0]) ? s_weather.sunset : "--";

  graphics_context_set_stroke_color(ctx, s_settings.primary_color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, GPoint(cx3, y_center), circle_radius);

  // Sunset glyph: semicircle + horizon + rays
  int sun_r = 12;
  int horizon_y = y_center - 2;

  // Draw full sun, then mask bottom half
  graphics_context_set_fill_color(ctx, s_settings.primary_color);
  graphics_fill_circle(ctx, GPoint(cx3, horizon_y), sun_r);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(cx3 - sun_r - 1, horizon_y, sun_r * 2 + 2, sun_r + 2), 0, GCornerNone);

  // Horizon line
  graphics_context_set_stroke_color(ctx, s_settings.primary_color);
  graphics_context_set_stroke_width(ctx, 1);
  int hz_w = sun_r + 8;
  graphics_draw_line(ctx, GPoint(cx3 - hz_w, horizon_y), GPoint(cx3 + hz_w, horizon_y));

  // Rays (up, 45° left, 45° right)
  int ray_start = sun_r + 3;
  int ray_end = sun_r + 7;
  graphics_draw_line(ctx, GPoint(cx3, horizon_y - ray_start),
                         GPoint(cx3, horizon_y - ray_end));
  int ds = ray_start * 7 / 10;
  int de = ray_end * 7 / 10;
  graphics_draw_line(ctx, GPoint(cx3 - ds, horizon_y - ds),
                         GPoint(cx3 - de, horizon_y - de));
  graphics_draw_line(ctx, GPoint(cx3 + ds, horizon_y - ds),
                         GPoint(cx3 + de, horizon_y - de));

  // Sunset time in lower half
  graphics_context_set_text_color(ctx, s_settings.primary_color);
  GRect set_rect = GRect(cx3 - circle_radius, y_center, circle_radius * 2, 24);
  graphics_draw_text(ctx, set, s_font_18, set_rect,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void prv_update_layout() {
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
  layer_set_frame(s_brand_layer,
                  GRect(0, group_y + 76, full.size.w, 20));
}

static void prv_unobstructed_change(AnimationProgress progress, void *context) {
  prv_update_layout();
}

static void prv_unobstructed_did_change(void *context) {
  prv_update_layout();
}

static void main_window_load(Window *window) {
  s_window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(s_window_layer);

  // Load resources
  s_brand_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PEBBLE_LOGO);
  s_font_14 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_14));
  s_font_18 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_18));
  s_font_28 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_28));
  s_font_68 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_68));

  // Top bar
  s_top_bar_layer = layer_create(GRect(0, 0, bounds.size.w, TOP_BAR_HEIGHT));
  layer_set_update_proc(s_top_bar_layer, top_bar_update_proc);

  // Time layer (positioned by prv_update_layout)
  s_time_layer = text_layer_create(GRect(0, 0, bounds.size.w, 76));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, s_settings.primary_color);
  text_layer_set_font(s_time_layer, s_font_68);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  // Brand text (positioned by prv_update_layout)
  s_brand_layer = layer_create(GRect(0, 0, bounds.size.w, 20));
  layer_set_update_proc(s_brand_layer, brand_update_proc);

  // Complications layer at the bottom
  int comp_height = 66;
  s_complications_layer = layer_create(GRect(0, bounds.size.h - comp_height - 4, bounds.size.w, comp_height));
  layer_set_update_proc(s_complications_layer, complications_update_proc);

  layer_add_child(s_window_layer, s_brand_layer);
  layer_add_child(s_window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(s_window_layer, s_complications_layer);
  layer_add_child(s_window_layer, s_top_bar_layer);

  UnobstructedAreaHandlers ua_handlers = {
    .change = prv_unobstructed_change,
    .did_change = prv_unobstructed_did_change
  };
  unobstructed_area_service_subscribe(ua_handlers, NULL);
  prv_update_layout();
}

static void main_window_unload(Window *window) {
  unobstructed_area_service_unsubscribe();
  layer_destroy(s_brand_layer);
  text_layer_destroy(s_time_layer);
  layer_destroy(s_complications_layer);
  layer_destroy(s_top_bar_layer);
  gbitmap_destroy(s_brand_bitmap);
  fonts_unload_custom_font(s_font_14);
  fonts_unload_custom_font(s_font_18);
  fonts_unload_custom_font(s_font_28);
  fonts_unload_custom_font(s_font_68);
}

static void init() {
  prv_load_settings();
  prv_load_weather();
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorWhite);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);

  update_time();
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  battery_state_service_subscribe(battery_callback);
  battery_callback(battery_state_service_peek());

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
