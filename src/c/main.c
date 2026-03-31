#include <pebble.h>

static Window *s_main_window;
static TextLayer *s_time_layer;
static Layer *s_complications_layer;
static Layer *s_top_bar_layer;

static GFont s_font_14;
static GFont s_font_18;
static GFont s_font_24;
static GFont s_font_28;
static GFont s_font_42;
static GFont s_font_56;

#define TOP_BAR_HEIGHT 20

static int s_battery_level;
static bool s_bt_connected;
static char s_status_buffer[24];

static void update_status_buffer();

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
  graphics_draw_text(ctx, "Pebble", s_font_14,
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
  // "67" top half
  GRect top_rect = GRect(cx1 - circle_radius, y_center - circle_radius + 2,
                          circle_radius * 2, circle_radius - 2);
  graphics_draw_text(ctx, "67", s_font_24,
                     top_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Divider line
  int line_margin = 10;
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(cx1 - circle_radius + line_margin, y_center),
                         GPoint(cx1 + circle_radius - line_margin, y_center));

  // "34" bottom half
  GRect bot_rect = GRect(cx1 - circle_radius, y_center + 1,
                          circle_radius * 2, circle_radius - 2);
  graphics_draw_text(ctx, "34", s_font_24,
                     bot_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // === Middle complication: filled black circle with "SF" ===
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, GPoint(cx2, y_center), circle_radius);

  graphics_context_set_text_color(ctx, GColorWhite);
  GRect mid_rect = GRect(cx2 - circle_radius, y_center - 16, circle_radius * 2, 34);
  graphics_draw_text(ctx, "SF", s_font_28,
                     mid_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // === Right complication: outlined circle with "67" ===
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, GPoint(cx3, y_center), circle_radius);

  graphics_context_set_text_color(ctx, GColorBlack);
  GRect right_rect = GRect(cx3 - circle_radius, y_center - 16, circle_radius * 2, 34);
  graphics_draw_text(ctx, "67", s_font_28,
                     right_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Load custom fonts
  s_font_14 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_14));
  s_font_18 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_18));
  s_font_24 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_24));
  s_font_28 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_28));
  s_font_42 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INTER_SEMIBOLD_42));
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
  fonts_unload_custom_font(s_font_18);
  fonts_unload_custom_font(s_font_24);
  fonts_unload_custom_font(s_font_28);
  fonts_unload_custom_font(s_font_42);
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
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
