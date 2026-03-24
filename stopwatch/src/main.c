#include <pebble.h>

#define TICK_MS 100
#define MAX_LAPS 20

static Window *s_window;
static TextLayer *s_time_layer;
static TextLayer *s_state_layer;
static TextLayer *s_lap_layers[3];
static AppTimer *s_timer;

static int s_elapsed_tenths = 0;  // tenths of seconds
static bool s_running = false;
static int s_laps[MAX_LAPS];
static int s_lap_count = 0;
static char s_time_buf[16];
static char s_lap_bufs[3][24];

static void format_time(int tenths, char *buf, size_t len) {
  int mins = tenths / 600;
  int secs = (tenths / 10) % 60;
  int t = tenths % 10;
  snprintf(buf, len, "%02d:%02d.%d", mins, secs, t);
}

static void update_display(void) {
  format_time(s_elapsed_tenths, s_time_buf, sizeof(s_time_buf));
  text_layer_set_text(s_time_layer, s_time_buf);

  text_layer_set_text(s_state_layer, s_running ? "Running" : (s_elapsed_tenths > 0 ? "Stopped" : "Ready"));

  // Show last 3 laps
  for (int i = 0; i < 3; i++) {
    int lap_idx = s_lap_count - 1 - i;
    if (lap_idx >= 0) {
      char time_str[12];
      format_time(s_laps[lap_idx], time_str, sizeof(time_str));
      snprintf(s_lap_bufs[i], sizeof(s_lap_bufs[i]), "Lap %d: %s", lap_idx + 1, time_str);
      text_layer_set_text(s_lap_layers[i], s_lap_bufs[i]);
    } else {
      text_layer_set_text(s_lap_layers[i], "");
    }
  }
}

static void timer_callback(void *data) {
  if (s_running) {
    s_elapsed_tenths++;
    update_display();
    s_timer = app_timer_register(TICK_MS, timer_callback, NULL);
  }
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  s_running = !s_running;
  if (s_running) {
    s_timer = app_timer_register(TICK_MS, timer_callback, NULL);
  }
  update_display();
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  if (s_running && s_lap_count < MAX_LAPS) {
    s_laps[s_lap_count++] = s_elapsed_tenths;
    update_display();
    vibes_short_pulse();
  }
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  if (!s_running) {
    s_elapsed_tenths = 0;
    s_lap_count = 0;
    update_display();
  }
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, GColorBlack);

  // State label
  s_state_layer = text_layer_create(GRect(0, 10, bounds.size.w, 20));
  text_layer_set_background_color(s_state_layer, GColorClear);
  text_layer_set_text_color(s_state_layer, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
  text_layer_set_font(s_state_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_state_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_state_layer));

  // Main time
  s_time_layer = text_layer_create(GRect(0, 32, bounds.size.w, 55));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  // Lap times (most recent 3)
  for (int i = 0; i < 3; i++) {
    s_lap_layers[i] = text_layer_create(GRect(10, 95 + i * 20, bounds.size.w - 20, 20));
    text_layer_set_background_color(s_lap_layers[i], GColorClear);
    text_layer_set_text_color(s_lap_layers[i], PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
    text_layer_set_font(s_lap_layers[i], fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(s_lap_layers[i], GTextAlignmentLeft);
    layer_add_child(root, text_layer_get_layer(s_lap_layers[i]));
  }

  update_display();
}

static void window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_state_layer);
  for (int i = 0; i < 3; i++) {
    text_layer_destroy(s_lap_layers[i]);
  }
}

static void init(void) {
  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

static void deinit(void) {
  if (s_timer) app_timer_cancel(s_timer);
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
