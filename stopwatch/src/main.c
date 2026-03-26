#include <pebble.h>

#define TICK_MS 100
#define MAX_LAPS_LIMIT 99  // absolute maximum array size

// AppMessage keys (must match messageKeys in package.json)
#define MSG_KEY_MAX_LAPS       0
#define MSG_KEY_SHOW_LAP_DELTAS 1

// Persist keys
#define KEY_ELAPSED    0
#define KEY_RUNNING    1
#define KEY_LAP_COUNT  2
#define KEY_LAPS       3  // 3..101 for laps[0..98]
#define KEY_LAP_START  102
#define KEY_LAP_DELTA  103
#define KEY_FIRST_LAUNCH 104
#define KEY_CFG_MAX_LAPS 105
#define KEY_CFG_SHOW_DELTAS 106

static Window *s_window;
static StatusBarLayer *s_status_bar;
static TextLayer *s_time_layer;
static TextLayer *s_state_layer;
static TextLayer *s_lap_layers[3];
static TextLayer *s_delta_layer;
static Layer *s_arc_layer;
static AppTimer *s_timer;

// First-launch overlay
static Layer *s_overlay_layer;
static TextLayer *s_overlay_text;
static bool s_show_overlay = false;

static int s_elapsed_tenths = 0;
static bool s_running = false;
static int s_laps[MAX_LAPS_LIMIT]; // individual lap durations (tenths)
static int s_lap_count = 0;
static int s_last_lap_start = 0;   // elapsed tenths at start of current lap
static int s_lap_delta = 0;        // delta vs previous lap (tenths)

// Configurable settings
static int s_max_laps = 20;        // configurable max laps (default 20)
static bool s_show_lap_deltas = true; // show +/- delta vs previous lap
static char s_time_buf[16];
static char s_lap_bufs[3][32];
static char s_delta_buf[16];

// Status bar offset
#define STATUS_BAR_HEIGHT 16

static void format_time(int tenths, char *buf, size_t len) {
  int mins = tenths / 600;
  int secs = (tenths / 10) % 60;
  int t = tenths % 10;
  snprintf(buf, len, "%02d:%02d.%d", mins, secs, t);
}

static void format_delta(int delta_tenths, char *buf, size_t len) {
  if (delta_tenths == 0) {
    buf[0] = '\0';
    return;
  }
  const char *sign = (delta_tenths > 0) ? "+" : "-";
  int abs_val = (delta_tenths < 0) ? -delta_tenths : delta_tenths;
  int secs = abs_val / 10;
  int t = abs_val % 10;
  snprintf(buf, len, "%s%d.%d", sign, secs, t);
}

static void update_display(void) {
  format_time(s_elapsed_tenths, s_time_buf, sizeof(s_time_buf));
  text_layer_set_text(s_time_layer, s_time_buf);

  text_layer_set_text(s_state_layer,
    s_running ? "Running" : (s_elapsed_tenths > 0 ? "Stopped" : "Ready"));

  // Show last 3 laps (individual durations)
  for (int i = 0; i < 3; i++) {
    int lap_idx = s_lap_count - 1 - i;
    if (lap_idx >= 0) {
      char time_str[16];
      format_time(s_laps[lap_idx], time_str, sizeof(time_str));
      snprintf(s_lap_bufs[i], sizeof(s_lap_bufs[i]), "Lap %d: %s", lap_idx + 1, time_str);
      text_layer_set_text(s_lap_layers[i], s_lap_bufs[i]);
    } else {
      text_layer_set_text(s_lap_layers[i], "");
    }
  }

  // Show lap delta (if enabled)
  if (s_show_lap_deltas && s_lap_count > 1 && s_lap_delta != 0) {
    format_delta(s_lap_delta, s_delta_buf, sizeof(s_delta_buf));
    text_layer_set_text(s_delta_layer, s_delta_buf);
    text_layer_set_text_color(s_delta_layer,
      PBL_IF_COLOR_ELSE(
        (s_lap_delta > 0) ? GColorRed : GColorGreen,
        GColorWhite));
  } else {
    text_layer_set_text(s_delta_layer, "");
  }

  // Redraw arc
  if (s_arc_layer) {
    layer_mark_dirty(s_arc_layer);
  }
}

// Draw circular elapsed-time arc (one full rotation per minute)
static void arc_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Arc center and radius - centered in the arc layer area
  GPoint center = GPoint(bounds.size.w / 2, bounds.size.h / 2);
  int radius = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2 - 4;

  // Background circle (dim ring)
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite));
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_circle(ctx, center, radius);

  // Elapsed arc: one full rotation per 60 seconds (600 tenths)
  int angle_deg = (s_elapsed_tenths % 600) * 360 / 600;
  if (angle_deg == 0 && s_elapsed_tenths == 0) return;

  // Convert to Pebble angle: 0 = top, clockwise, TRIG_MAX_ANGLE = 360 deg
  int32_t start_angle = 0;
  int32_t end_angle = (int32_t)angle_deg * TRIG_MAX_ANGLE / 360;
  if (end_angle == 0) end_angle = TRIG_MAX_ANGLE;  // full circle at 0 seconds

  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorCyan, GColorWhite));

  // Draw filled arc using graphics_fill_radial
  graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle,
                       6, start_angle, end_angle);
}

static void record_lap(void) {
  int current_lap_time = s_elapsed_tenths - s_last_lap_start;
  s_lap_delta = (s_lap_count > 0) ? current_lap_time - s_laps[s_lap_count - 1] : 0;
  s_laps[s_lap_count++] = current_lap_time;
  s_last_lap_start = s_elapsed_tenths;
}

static void timer_callback(void *data) {
  if (s_running) {
    s_elapsed_tenths++;
    update_display();
    s_timer = app_timer_register(TICK_MS, timer_callback, NULL);
  }
}

static void dismiss_overlay(void) {
  if (s_show_overlay && s_overlay_layer) {
    s_show_overlay = false;
    layer_set_hidden(s_overlay_layer, true);
  }
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  if (s_show_overlay) {
    dismiss_overlay();
    return;
  }
  s_running = !s_running;
  if (s_running) {
    s_timer = app_timer_register(TICK_MS, timer_callback, NULL);
  }
  update_display();
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  if (s_show_overlay) {
    dismiss_overlay();
    return;
  }
  if (s_running && s_lap_count < s_max_laps) {
    record_lap();
    update_display();
    vibes_short_pulse();
  }
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  if (s_show_overlay) {
    dismiss_overlay();
    return;
  }
  if (!s_running) {
    s_elapsed_tenths = 0;
    s_lap_count = 0;
    s_last_lap_start = 0;
    s_lap_delta = 0;
    update_display();
  }
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

// BT connection handler
static void bt_handler(bool connected) {
  if (!connected) {
    vibes_double_pulse();
  }
}

// Overlay draw callback (semi-transparent background)
static void overlay_update_proc(Layer *layer, GContext *ctx) {
  if (!s_show_overlay) return;
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, GColorBlack);

  // 1. Status bar with dotted separator
  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, GColorBlack,
    PBL_IF_COLOR_ELSE(GColorWhite, GColorWhite));
  status_bar_layer_set_separator_mode(s_status_bar,
    StatusBarLayerSeparatorModeDotted);
  layer_add_child(root, status_bar_layer_get_layer(s_status_bar));

  // Content area starts below status bar
  int y_offset = STATUS_BAR_HEIGHT;
  int content_w = bounds.size.w;

  // 2. Arc canvas layer (behind text, in main content area)
  GRect arc_rect = GRect(
    (content_w - 120) / 2, y_offset + 20,
    120, 120);
  // On emery (200px wide), make arc bigger
  #ifdef PBL_PLATFORM_EMERY
  arc_rect = GRect((content_w - 150) / 2, y_offset + 15, 150, 150);
  #endif
  s_arc_layer = layer_create(arc_rect);
  layer_set_update_proc(s_arc_layer, arc_update_proc);
  layer_add_child(root, s_arc_layer);

  // State label
  s_state_layer = text_layer_create(GRect(0, y_offset + 2, content_w, 20));
  text_layer_set_background_color(s_state_layer, GColorClear);
  text_layer_set_text_color(s_state_layer,
    PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
  text_layer_set_font(s_state_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_state_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_state_layer));

  // Main time (centered over the arc)
  s_time_layer = text_layer_create(GRect(0, y_offset + 42, content_w, 55));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer,
    fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  // Lap delta display (right below the time)
  s_delta_layer = text_layer_create(GRect(0, y_offset + 90, content_w, 20));
  text_layer_set_background_color(s_delta_layer, GColorClear);
  text_layer_set_text_color(s_delta_layer, GColorWhite);
  text_layer_set_font(s_delta_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_delta_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_delta_layer));

  // Lap times (most recent 3)
  int lap_y = y_offset + 110;
  for (int i = 0; i < 3; i++) {
    s_lap_layers[i] = text_layer_create(
      GRect(10, lap_y + i * 18, content_w - 20, 18));
    text_layer_set_background_color(s_lap_layers[i], GColorClear);
    text_layer_set_text_color(s_lap_layers[i],
      PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
    text_layer_set_font(s_lap_layers[i],
      fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(s_lap_layers[i], GTextAlignmentLeft);
    layer_add_child(root, text_layer_get_layer(s_lap_layers[i]));
  }

  // 5. First-launch overlay
  if (s_show_overlay) {
    s_overlay_layer = layer_create(bounds);
    layer_set_update_proc(s_overlay_layer, overlay_update_proc);
    layer_add_child(root, s_overlay_layer);

    s_overlay_text = text_layer_create(
      GRect(10, bounds.size.h / 2 - 45, bounds.size.w - 20, 90));
    text_layer_set_background_color(s_overlay_text, GColorClear);
    text_layer_set_text_color(s_overlay_text,
      PBL_IF_COLOR_ELSE(GColorCyan, GColorWhite));
    text_layer_set_font(s_overlay_text,
      fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_text_alignment(s_overlay_text, GTextAlignmentCenter);
    text_layer_set_text(s_overlay_text,
      "Stopwatch v1.1\n"
      "SEL: Start/Stop\n"
      "UP: Lap\n"
      "DOWN: Reset\n"
      "Press any button");
    layer_add_child(s_overlay_layer, text_layer_get_layer(s_overlay_text));
  }

  update_display();
}

static void window_unload(Window *window) {
  status_bar_layer_destroy(s_status_bar);
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_state_layer);
  text_layer_destroy(s_delta_layer);
  layer_destroy(s_arc_layer);
  for (int i = 0; i < 3; i++) {
    text_layer_destroy(s_lap_layers[i]);
  }
  if (s_overlay_layer) {
    if (s_overlay_text) text_layer_destroy(s_overlay_text);
    layer_destroy(s_overlay_layer);
  }
}

// 4. Persist state
static void save_state(void) {
  persist_write_int(KEY_ELAPSED, s_elapsed_tenths);
  persist_write_bool(KEY_RUNNING, s_running);
  persist_write_int(KEY_LAP_COUNT, s_lap_count);
  persist_write_int(KEY_LAP_START, s_last_lap_start);
  persist_write_int(KEY_LAP_DELTA, s_lap_delta);
  for (int i = 0; i < s_lap_count && i < MAX_LAPS_LIMIT; i++) {
    persist_write_int(KEY_LAPS + i, s_laps[i]);
  }
  persist_write_int(KEY_CFG_MAX_LAPS, s_max_laps);
  persist_write_bool(KEY_CFG_SHOW_DELTAS, s_show_lap_deltas);
}

static void load_state(void) {
  if (persist_exists(KEY_ELAPSED)) {
    s_elapsed_tenths = persist_read_int(KEY_ELAPSED);
    s_running = persist_read_bool(KEY_RUNNING);
    s_lap_count = persist_read_int(KEY_LAP_COUNT);
    s_last_lap_start = persist_read_int(KEY_LAP_START);
    s_lap_delta = persist_read_int(KEY_LAP_DELTA);
    for (int i = 0; i < s_lap_count && i < MAX_LAPS_LIMIT; i++) {
      s_laps[i] = persist_read_int(KEY_LAPS + i);
    }
  }
  if (persist_exists(KEY_CFG_MAX_LAPS)) {
    s_max_laps = persist_read_int(KEY_CFG_MAX_LAPS);
  }
  if (persist_exists(KEY_CFG_SHOW_DELTAS)) {
    s_show_lap_deltas = persist_read_bool(KEY_CFG_SHOW_DELTAS);
  }
}

// AppMessage: receive config from phone
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *max_laps_t = dict_find(iter, MSG_KEY_MAX_LAPS);
  if (max_laps_t) {
    int val = max_laps_t->value->int32;
    if (val >= 1 && val <= MAX_LAPS_LIMIT) {
      s_max_laps = val;
      persist_write_int(KEY_CFG_MAX_LAPS, s_max_laps);
    }
  }

  Tuple *show_deltas_t = dict_find(iter, MSG_KEY_SHOW_LAP_DELTAS);
  if (show_deltas_t) {
    s_show_lap_deltas = (show_deltas_t->value->int32 != 0);
    persist_write_bool(KEY_CFG_SHOW_DELTAS, s_show_lap_deltas);
    update_display();
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", reason);
}

static void init(void) {
  // Check first launch
  if (!persist_exists(KEY_FIRST_LAUNCH)) {
    s_show_overlay = true;
    persist_write_bool(KEY_FIRST_LAUNCH, true);
  }

  // Load persisted state
  load_state();

  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  // Resume timer if was running
  if (s_running) {
    s_timer = app_timer_register(TICK_MS, timer_callback, NULL);
  }

  // 5. BT disconnect alert
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bt_handler,
  });

  // Set up AppMessage for config
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_open(128, 64);
}

static void deinit(void) {
  // 4. Persist on exit
  save_state();

  connection_service_unsubscribe();
  if (s_timer) app_timer_cancel(s_timer);
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
