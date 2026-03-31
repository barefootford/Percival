#include <pebble.h>

extern uint32_t MESSAGE_KEY_TEMPERATURE;
extern uint32_t MESSAGE_KEY_TEMP_HIGH;
extern uint32_t MESSAGE_KEY_TEMP_LOW;

static Window *s_main_window;
static TextLayer *s_time_layer;
static Layer *s_complications_layer;
static Layer *s_top_bar_layer;

static GFont s_font_14;
static GFont s_font_24;
static GFont s_font_28;
static GFont s_font_56;

#define TOP_BAR_HEIGHT 20

static int s_battery_level;
static bool s_bt_connected;
static char s_status_buffer[24];

static bool s_weather_loaded;
static char s_temp_buffer[8];
static char s_high_buffer[8];
static char s_low_buffer[8];

static void update_status_buffer();

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
  Tuple *high_tuple = dict_find(iterator, MESSAGE_KEY_TEMP_HIGH);
  Tuple *low_tuple = dict_find(iterator, MESSAGE_KEY_TEMP_LOW);

  if (temp_tuple) {
    snprintf(s_temp_buffer, sizeof(s_temp_buffer), "%d", (int)temp_tuple->value->int32);
  }
  if (high_tuple) {
    snprintf(s_high_buffer, sizeof(s_high_buffer), "%d", (int)high_tuple->value->int32);
  }
  if (low_tuple) {
    snprintf(s_low_buffer, sizeof(s_low_buffer), "%d", (int)low_tuple->value->int32);
  }

  if (temp_tuple || high_tuple || low_tuple) {
    s_weather_loaded = true;
    if (s_complications_layer) {
      layer_mark_dirty(s_complications_layer);
    }
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
  text_layer_set_text(s_time_layer, s_time_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  update_status_buffer();

  // Request weather update every 30 minutes
  if (tick_time->tm_min % 30 == 0) {
    DictionaryIterator *iter;
    if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
      dict_write_uint8(iter, 0, 0);
      app_message_outbox_send();
    }
  }
}

static void update_status_buffer() {
  HealthMetric metric = HealthMetricStepCount;
  int steps = (int)health_service_sum_today(metric);
  snprintf(s_status_buffer, sizeof(s_status_buffer), "%s | %d | %d%%",
           s_bt_connected ? "O" : "X", steps, s_battery_level);
  if (s_top_bar_layer) {
    layer_mark_dirty(s_top_bar_layer);
  }
}

static void battery_callback(BatteryChargeState state) {
  s_battery_level = state.charge_percent;
  update_status_buffer();
}

static void bluetooth_callback(bool connected) {
  s_bt_connected = connected;
  if (!connected) {
    vibes_double_pulse();
  }
  update_status_buffer();
}

static void top_bar_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Black background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // "PEBBLE" left-aligned
  graphics_context_set_text_color(ctx, GColorWhite);
  GRect left_rect = GRect(4, 2, bounds.size.w / 4, bounds.size.h);
  graphics_draw_text(ctx, "PEBBLE", s_font_14,
                     left_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Right-aligned status text
  GRect right_rect = GRect(bounds.size.w / 4, 2, bounds.size.w * 3 / 4 - 4, bounds.size.h);
  graphics_draw_text(ctx, s_status_buffer, s_font_14,
                     right_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
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

  // === Left complication: 67/34 fraction ===
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, GPoint(cx1, y_center), circle_radius);

  graphics_context_set_text_color(ctx, GColorBlack);
  GRect top_rect = GRect(cx1 - circle_radius, y_center - circle_radius + 2,
                          circle_radius * 2, circle_radius - 2);
  graphics_draw_text(ctx, s_weather_loaded ? s_high_buffer : "--", s_font_24,
                     top_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Divider line
  int line_margin = 10;
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(cx1 - circle_radius + line_margin, y_center),
                         GPoint(cx1 + circle_radius - line_margin, y_center));

  GRect bot_rect = GRect(cx1 - circle_radius, y_center + 1,
                          circle_radius * 2, circle_radius - 2);
  graphics_draw_text(ctx, s_weather_loaded ? s_low_buffer : "--", s_font_24,
                     bot_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // === Middle complication: filled black circle with "SF" ===
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, GPoint(cx2, y_center), circle_radius);

  graphics_context_set_text_color(ctx, GColorWhite);
  GRect mid_rect = GRect(cx2 - circle_radius, y_center - 16, circle_radius * 2, 34);
  graphics_draw_text(ctx, "SF", s_font_28,
                     mid_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // === Right complication: outlined circle with current temp ===
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, GPoint(cx3, y_center), circle_radius);

  graphics_context_set_text_color(ctx, GColorBlack);
  GRect right_rect = GRect(cx3 - circle_radius, y_center - 16, circle_radius * 2, 34);
  graphics_draw_text(ctx, s_weather_loaded ? s_temp_buffer : "--", s_font_28,
                     right_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Load custom fonts
  s_font_14 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_14));
  s_font_24 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_24));
  s_font_28 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_28));
  s_font_56 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_56));

  // Top bar
  s_top_bar_layer = layer_create(GRect(0, 0, bounds.size.w, TOP_BAR_HEIGHT));
  layer_set_update_proc(s_top_bar_layer, top_bar_update_proc);

  // Time layer - large, centered in the upper portion
  int time_y = (bounds.size.h - 56) / 2 - 20;
  s_time_layer = text_layer_create(GRect(0, time_y, bounds.size.w, 64));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_font(s_time_layer, s_font_56);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  // Complications layer at the bottom
  int comp_height = 66;
  s_complications_layer = layer_create(GRect(0, bounds.size.h - comp_height - 4, bounds.size.w, comp_height));
  layer_set_update_proc(s_complications_layer, complications_update_proc);

  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(window_layer, s_complications_layer);
  layer_add_child(window_layer, s_top_bar_layer);
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  layer_destroy(s_complications_layer);
  layer_destroy(s_top_bar_layer);
  fonts_unload_custom_font(s_font_14);
  fonts_unload_custom_font(s_font_24);
  fonts_unload_custom_font(s_font_28);
  fonts_unload_custom_font(s_font_56);
}

static void init() {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorLightGray);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);

  update_time();
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  battery_state_service_subscribe(battery_callback);
  battery_callback(battery_state_service_peek());

  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bluetooth_callback
  });
  s_bt_connected = connection_service_peek_pebble_app_connection();
  update_status_buffer();

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  app_message_open(128, 128);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
