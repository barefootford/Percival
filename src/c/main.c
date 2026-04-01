#include <pebble.h>

extern uint32_t MESSAGE_KEY_TEMPERATURE;
extern uint32_t MESSAGE_KEY_TEMP_HIGH;
extern uint32_t MESSAGE_KEY_TEMP_LOW;
extern uint32_t MESSAGE_KEY_CITY;
extern uint32_t MESSAGE_KEY_PrimaryColor;

static Window *s_main_window;
static TextLayer *s_time_layer;
static Layer *s_brand_layer;
static Layer *s_complications_layer;
static Layer *s_top_bar_layer;

static GFont s_font_14;
static GFont s_font_16;
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

  if (temp_tuple || high_tuple || low_tuple || city_tuple) {
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
  const char *text = "PEBBLE";
  int len = 6;
  int tracking = 3;

  // Measure each character and total width
  int total_width = 0;
  GSize char_sizes[6];
  GRect measure_rect = GRect(0, 0, bounds.size.w, bounds.size.h);
  for (int i = 0; i < len; i++) {
    char ch[2] = {text[i], '\0'};
    char_sizes[i] = graphics_text_layout_get_content_size(
        ch, s_font_16, measure_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
    total_width += char_sizes[i].w;
  }
  total_width += tracking * (len - 1);

  // Draw each character centered
  int x = (bounds.size.w - total_width) / 2;
  graphics_context_set_text_color(ctx, s_settings.primary_color);
  for (int i = 0; i < len; i++) {
    char ch[2] = {text[i], '\0'};
    GRect char_rect = GRect(x, 0, char_sizes[i].w, bounds.size.h);
    graphics_draw_text(ctx, ch, s_font_16, char_rect,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    x += char_sizes[i].w + tracking;
  }
}

static void top_bar_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, s_settings.primary_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  graphics_context_set_text_color(ctx, GColorWhite);
  GRect text_rect = GRect(4, 2, bounds.size.w - 8, bounds.size.h);
  graphics_draw_text(ctx, s_status_buffer, s_font_14,
                     text_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}

static void complications_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  int circle_radius = 30;
  int y_center = bounds.size.h / 2;

  // Three circles evenly distributed across width
  int section_w = bounds.size.w / 3;
  int cx1 = section_w / 2;
  int cx2 = section_w + section_w / 2;
  int cx3 = 2 * section_w + section_w / 2;

  // === Left complication: high/low fraction ===
  graphics_context_set_stroke_color(ctx, s_settings.primary_color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, GPoint(cx1, y_center), circle_radius);

  graphics_context_set_text_color(ctx, s_settings.primary_color);
  GRect top_rect = GRect(cx1 - circle_radius, y_center - circle_radius + 6,
                          circle_radius * 2, circle_radius - 6);
  graphics_draw_text(ctx, s_weather.loaded ? s_weather.high : "--", s_font_18,
                     top_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Divider line
  int line_margin = 10;
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(cx1 - circle_radius + line_margin, y_center),
                         GPoint(cx1 + circle_radius - line_margin, y_center));

  GRect bot_rect = GRect(cx1 - circle_radius, y_center + 1,
                          circle_radius * 2, circle_radius - 2);
  graphics_draw_text(ctx, s_weather.loaded ? s_weather.low : "--", s_font_18,
                     bot_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // === Middle complication: city initials ===
  graphics_context_set_stroke_color(ctx, s_settings.primary_color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, GPoint(cx2, y_center), circle_radius);

  graphics_context_set_text_color(ctx, s_settings.primary_color);
  GRect mid_rect = GRect(cx2 - circle_radius, y_center - 16, circle_radius * 2, 34);
  graphics_draw_text(ctx, s_weather.loaded ? s_weather.city : "--", s_font_28,
                     mid_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // === Right complication: outlined circle with current temp ===
  graphics_context_set_stroke_color(ctx, s_settings.primary_color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, GPoint(cx3, y_center), circle_radius);

  graphics_context_set_text_color(ctx, s_settings.primary_color);
  GRect right_rect = GRect(cx3 - circle_radius, y_center - 16, circle_radius * 2, 34);
  graphics_draw_text(ctx, s_weather.loaded ? s_weather.temp : "--", s_font_28,
                     right_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  if (s_weather.loaded) {
    GSize temp_size = graphics_text_layout_get_content_size(
        s_weather.temp, s_font_28, right_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
    int dot_x = cx3 + temp_size.w / 2 + 3;
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_circle(ctx, GPoint(dot_x, right_rect.origin.y + 7), 2);
  }
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Load custom fonts
  s_font_14 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_14));
  s_font_16 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_16));
  s_font_18 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_18));
  s_font_28 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_28));
  s_font_68 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_68));

  // Top bar
  s_top_bar_layer = layer_create(GRect(0, 0, bounds.size.w, TOP_BAR_HEIGHT));
  layer_set_update_proc(s_top_bar_layer, top_bar_update_proc);

  // Time + brand as a vertically centered group
  int comp_top = bounds.size.h - 66 - 4;
  int group_height = 76 + 18;  // time + brand
  int group_y = TOP_BAR_HEIGHT + (comp_top - TOP_BAR_HEIGHT - group_height) / 2;

  // Time layer
  int time_y = group_y;
  s_time_layer = text_layer_create(GRect(0, time_y, bounds.size.w, 76));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, s_settings.primary_color);
  text_layer_set_font(s_time_layer, s_font_68);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  // Brand text - centered below time
  s_brand_layer = layer_create(GRect(0, time_y + 76, bounds.size.w, 20));
  layer_set_update_proc(s_brand_layer, brand_update_proc);

  // Complications layer at the bottom
  int comp_height = 66;
  s_complications_layer = layer_create(GRect(0, bounds.size.h - comp_height - 4, bounds.size.w, comp_height));
  layer_set_update_proc(s_complications_layer, complications_update_proc);

  layer_add_child(window_layer, s_brand_layer);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(window_layer, s_complications_layer);
  layer_add_child(window_layer, s_top_bar_layer);
}

static void main_window_unload(Window *window) {
  layer_destroy(s_brand_layer);
  text_layer_destroy(s_time_layer);
  layer_destroy(s_complications_layer);
  layer_destroy(s_top_bar_layer);
  fonts_unload_custom_font(s_font_14);
  fonts_unload_custom_font(s_font_16);
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
