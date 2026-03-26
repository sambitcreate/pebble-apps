#include <pebble.h>

// ---- Persist keys ----
#define PERSIST_DIE_TYPE    0
#define PERSIST_STATS_BASE  10  // 10..15 = total_rolls per die type
#define PERSIST_SUM_BASE    20  // 20..25 = sum per die type
#define PERSIST_MIN_BASE    30  // 30..35 = min per die type
#define PERSIST_MAX_BASE    40  // 40..45 = max per die type
#define PERSIST_FIRST_LAUNCH 99
#define PERSIST_SHOW_STATS   100

// ---- AppMessage keys (must match messageKeys in package.json) ----
#define MSG_KEY_DIE_TYPE   0
#define MSG_KEY_SHOW_STATS 1

static Window *s_window;
static StatusBarLayer *s_status_bar;
static Layer *s_die_layer;
static TextLayer *s_result_layer;
static TextLayer *s_type_layer;
static TextLayer *s_history_layer;
static TextLayer *s_combo_layer;
static TextLayer *s_stats_layer;

typedef enum {
  DIE_D4 = 0,
  DIE_D6,
  DIE_D8,
  DIE_D10,
  DIE_D12,
  DIE_D20,
  DIE_COUNT,
} DieType;

static const int DIE_SIDES[] = {4, 6, 8, 10, 12, 20};
static const char *DIE_NAMES[] = {"D4", "D6", "D8", "D10", "D12", "D20"};

static DieType s_die_type = DIE_D6;
static int s_result = 0;
static int s_history[5] = {0};
static int s_history_count = 0;
static char s_result_buf[8];
static char s_type_buf[16];
static char s_history_buf[40];

// ---- Spin animation ----
static int s_spin_count = 0;
#define SPIN_FRAMES 12
static AppTimer *s_spin_timer = NULL;
static int s_actual_result = 0;
static bool s_spinning = false;

// ---- Combo streak ----
static int s_last_result = 0;
static int s_combo_count = 0;
static char s_combo_buf[16];

// ---- Roll statistics per die type ----
typedef struct {
  int total_rolls;
  int sum;
  int min;
  int max;
} DieStats;

static DieStats s_stats[DIE_COUNT];
static char s_stats_buf[80];
static bool s_stats_visible = false;

// ---- BT disconnect ----
static void bt_handler(bool connected) {
  if (!connected) {
    vibes_double_pulse();
  }
}

// ---- First-launch overlay ----
static TextLayer *s_overlay_layer = NULL;
static AppTimer *s_overlay_timer = NULL;

static void dismiss_overlay(void *data) {
  if (s_overlay_layer) {
    text_layer_destroy(s_overlay_layer);
    s_overlay_layer = NULL;
  }
  s_overlay_timer = NULL;
}

static void show_first_launch_overlay(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_overlay_layer = text_layer_create(GRect(10, bounds.size.h / 2 - 40, bounds.size.w - 20, 80));
  text_layer_set_background_color(s_overlay_layer, GColorBlack);
  text_layer_set_text_color(s_overlay_layer, GColorWhite);
  text_layer_set_font(s_overlay_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_overlay_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_overlay_layer, GTextOverflowModeWordWrap);
  text_layer_set_text(s_overlay_layer, "Shake or press Select to roll!\nUp/Down: change die");
  layer_add_child(root, text_layer_get_layer(s_overlay_layer));

  s_overlay_timer = app_timer_register(3000, dismiss_overlay, NULL);
  persist_write_bool(PERSIST_FIRST_LAUNCH, true);
}

// ---- Statistics persistence ----
static void load_stats(void) {
  for (int i = 0; i < DIE_COUNT; i++) {
    if (persist_exists(PERSIST_STATS_BASE + i)) {
      s_stats[i].total_rolls = persist_read_int(PERSIST_STATS_BASE + i);
      s_stats[i].sum = persist_read_int(PERSIST_SUM_BASE + i);
      s_stats[i].min = persist_read_int(PERSIST_MIN_BASE + i);
      s_stats[i].max = persist_read_int(PERSIST_MAX_BASE + i);
    } else {
      s_stats[i].total_rolls = 0;
      s_stats[i].sum = 0;
      s_stats[i].min = 0;
      s_stats[i].max = 0;
    }
  }
}

static void save_stats(void) {
  for (int i = 0; i < DIE_COUNT; i++) {
    persist_write_int(PERSIST_STATS_BASE + i, s_stats[i].total_rolls);
    persist_write_int(PERSIST_SUM_BASE + i, s_stats[i].sum);
    persist_write_int(PERSIST_MIN_BASE + i, s_stats[i].min);
    persist_write_int(PERSIST_MAX_BASE + i, s_stats[i].max);
  }
}

static void update_stats(int result) {
  DieStats *st = &s_stats[s_die_type];
  st->total_rolls++;
  st->sum += result;
  if (st->total_rolls == 1) {
    st->min = result;
    st->max = result;
  } else {
    if (result < st->min) st->min = result;
    if (result > st->max) st->max = result;
  }
}

static void show_stats(void) {
  DieStats *st = &s_stats[s_die_type];
  if (st->total_rolls == 0) {
    snprintf(s_stats_buf, sizeof(s_stats_buf), "%s: No rolls yet", DIE_NAMES[s_die_type]);
  } else {
    int avg_x10 = (st->sum * 10) / st->total_rolls;
    snprintf(s_stats_buf, sizeof(s_stats_buf), "%s: %d rolls avg:%d.%d min:%d max:%d",
             DIE_NAMES[s_die_type], st->total_rolls,
             avg_x10 / 10, avg_x10 % 10,
             st->min, st->max);
  }
  text_layer_set_text(s_stats_layer, s_stats_buf);
  s_stats_visible = true;
}

static void hide_stats(void) {
  text_layer_set_text(s_stats_layer, "");
  s_stats_visible = false;
}

// ---- Drawing ----
static void draw_d6_pips(GContext *ctx, int value, GRect face) {
  int cx = face.origin.x + face.size.w / 2;
  int cy = face.origin.y + face.size.h / 2;
  int dx = face.size.w / 4;
  int dy = face.size.h / 4;
  int r = 5;

  GColor pip = PBL_IF_COLOR_ELSE(GColorBlack, GColorBlack);
  graphics_context_set_fill_color(ctx, pip);

  // Center pip
  if (value == 1 || value == 3 || value == 5) {
    graphics_fill_circle(ctx, GPoint(cx, cy), r);
  }
  // Top-left, bottom-right
  if (value >= 2) {
    graphics_fill_circle(ctx, GPoint(cx - dx, cy - dy), r);
    graphics_fill_circle(ctx, GPoint(cx + dx, cy + dy), r);
  }
  // Top-right, bottom-left
  if (value >= 4) {
    graphics_fill_circle(ctx, GPoint(cx + dx, cy - dy), r);
    graphics_fill_circle(ctx, GPoint(cx - dx, cy + dy), r);
  }
  // Middle-left, middle-right
  if (value == 6) {
    graphics_fill_circle(ctx, GPoint(cx - dx, cy), r);
    graphics_fill_circle(ctx, GPoint(cx + dx, cy), r);
  }
}

static void die_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int size = 80;
  int x = (bounds.size.w - size) / 2;
  int y = 10;
  GRect face = GRect(x, y, size, size);

  if (s_result == 0) return;

  if (s_die_type == DIE_D6) {
    // Draw D6 face with pips
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, face, 8, GCornersAll);
    graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_round_rect(ctx, face, 8);
    draw_d6_pips(ctx, s_result, face);
  } else {
    // Draw generic die shape
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite));
    if (s_die_type == DIE_D4) {
      // Triangle
      GPoint tri[] = {
        GPoint(x + size/2, y),
        GPoint(x, y + size),
        GPoint(x + size, y + size),
      };
      GPathInfo info = { .num_points = 3, .points = tri };
      GPath *path = gpath_create(&info);
      gpath_draw_filled(ctx, path);
      graphics_context_set_stroke_color(ctx, GColorBlack);
      gpath_draw_outline(ctx, path);
      gpath_destroy(path);
    } else {
      // Rounded rect for all others
      graphics_fill_rect(ctx, face, 4, GCornersAll);
      graphics_context_set_stroke_color(ctx, GColorBlack);
      graphics_draw_round_rect(ctx, face, 4);
    }

    // Draw number in center
    snprintf(s_result_buf, sizeof(s_result_buf), "%d", s_result);
    graphics_context_set_text_color(ctx, s_die_type == DIE_D6 ? GColorBlack :
                                    PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
    graphics_draw_text(ctx, s_result_buf,
                       fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS),
                       GRect(x, y + size/2 - 22, size, 44),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }
}

static void update_history_text(void) {
  s_history_buf[0] = '\0';
  int count = s_history_count > 5 ? 5 : s_history_count;
  for (int i = 0; i < count; i++) {
    char tmp[8];
    snprintf(tmp, sizeof(tmp), "%s%d", i > 0 ? " " : "", s_history[i]);
    strcat(s_history_buf, tmp);
  }
  text_layer_set_text(s_history_layer, s_history_buf);
}

static void update_combo(int result) {
  if (result == s_last_result) {
    s_combo_count++;
  } else {
    s_combo_count = 1;
    s_last_result = result;
  }
  if (s_combo_count >= 2) {
    snprintf(s_combo_buf, sizeof(s_combo_buf), "x%d!", s_combo_count);
    text_layer_set_text(s_combo_layer, s_combo_buf);
  } else {
    text_layer_set_text(s_combo_layer, "");
  }
}

static void finalize_roll(void) {
  s_result = s_actual_result;

  // Shift history
  if (s_history_count < 5) {
    s_history[s_history_count++] = s_result;
  } else {
    for (int i = 0; i < 4; i++) s_history[i] = s_history[i + 1];
    s_history[4] = s_result;
  }

  snprintf(s_result_buf, sizeof(s_result_buf), "%d", s_result);
  text_layer_set_text(s_result_layer, s_result_buf);
  update_history_text();
  update_combo(s_result);
  update_stats(s_result);
  layer_mark_dirty(s_die_layer);

  // Hide stats if visible
  if (s_stats_visible) hide_stats();

  vibes_short_pulse();
  s_spinning = false;
}

// ---- Spin animation ----
static void spin_frame(void *data) {
  s_result = (rand() % DIE_SIDES[s_die_type]) + 1;
  s_spin_count++;
  layer_mark_dirty(s_die_layer);

  // Update the result text during spin too for visual feedback
  snprintf(s_result_buf, sizeof(s_result_buf), "%d", s_result);
  text_layer_set_text(s_result_layer, s_result_buf);

  if (s_spin_count < SPIN_FRAMES) {
    int delay = 40 + (s_spin_count * s_spin_count * 3); // quadratic ramp-down: 40ms -> 403ms (~2s total)
    s_spin_timer = app_timer_register(delay, spin_frame, NULL);
  } else {
    s_spin_timer = NULL;
    finalize_roll();
  }
}

static void roll(void) {
  if (s_spinning) return; // Don't roll while spinning

  // Dismiss first-launch overlay if still showing
  if (s_overlay_layer) {
    if (s_overlay_timer) {
      app_timer_cancel(s_overlay_timer);
      s_overlay_timer = NULL;
    }
    dismiss_overlay(NULL);
  }

  // Determine the actual result up front
  s_actual_result = (rand() % DIE_SIDES[s_die_type]) + 1;

  // Start spin animation
  s_spinning = true;
  s_spin_count = 0;
  s_spin_timer = app_timer_register(30, spin_frame, NULL);
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  roll();
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  roll();
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  if (s_spinning) return;
  s_die_type = (s_die_type + 1) % DIE_COUNT;
  snprintf(s_type_buf, sizeof(s_type_buf), "[ %s ]", DIE_NAMES[s_die_type]);
  text_layer_set_text(s_type_layer, s_type_buf);
  s_result = 0;
  text_layer_set_text(s_result_layer, "");
  text_layer_set_text(s_combo_layer, "");
  s_last_result = 0;
  s_combo_count = 0;
  layer_mark_dirty(s_die_layer);
  if (s_stats_visible) hide_stats();
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  if (s_spinning) return;
  s_die_type = (s_die_type + DIE_COUNT - 1) % DIE_COUNT;
  snprintf(s_type_buf, sizeof(s_type_buf), "[ %s ]", DIE_NAMES[s_die_type]);
  text_layer_set_text(s_type_layer, s_type_buf);
  s_result = 0;
  text_layer_set_text(s_result_layer, "");
  text_layer_set_text(s_combo_layer, "");
  s_last_result = 0;
  s_combo_count = 0;
  layer_mark_dirty(s_die_layer);
  if (s_stats_visible) hide_stats();
}

static void down_long_click(ClickRecognizerRef recognizer, void *context) {
  if (s_spinning) return;
  if (s_stats_visible) {
    hide_stats();
  } else {
    show_stats();
  }
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_long_click_subscribe(BUTTON_ID_DOWN, 500, down_long_click, NULL);
}

// ---- AppMessage handlers ----
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *die_type_t = dict_find(iter, MSG_KEY_DIE_TYPE);
  if (die_type_t) {
    int val = die_type_t->value->int32;
    if (val >= 0 && val < DIE_COUNT) {
      s_die_type = (DieType)val;
      persist_write_int(PERSIST_DIE_TYPE, (int)s_die_type);
      snprintf(s_type_buf, sizeof(s_type_buf), "[ %s ]", DIE_NAMES[s_die_type]);
      text_layer_set_text(s_type_layer, s_type_buf);
      s_result = 0;
      text_layer_set_text_color(s_result_layer, GColorWhite);
      text_layer_set_text(s_result_layer, "");
      text_layer_set_text(s_combo_layer, "");
      s_last_result = 0;
      s_combo_count = 0;
      layer_mark_dirty(s_die_layer);
      if (s_stats_visible) hide_stats();
    }
  }

  Tuple *show_stats_t = dict_find(iter, MSG_KEY_SHOW_STATS);
  if (show_stats_t) {
    bool want_stats = (show_stats_t->value->int32 != 0);
    persist_write_bool(PERSIST_SHOW_STATS, want_stats);
    if (want_stats) {
      show_stats();
    } else {
      hide_stats();
    }
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", reason);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, GColorBlack);

  // Status bar at top (16px)
  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, GColorBlack, GColorWhite);
  status_bar_layer_set_separator_mode(s_status_bar, StatusBarLayerSeparatorModeDotted);
  layer_add_child(root, status_bar_layer_get_layer(s_status_bar));

  int y_offset = STATUS_BAR_LAYER_HEIGHT; // 16px shift

  // Die type label
  s_type_layer = text_layer_create(GRect(0, y_offset, bounds.size.w, 24));
  text_layer_set_background_color(s_type_layer, GColorClear);
  text_layer_set_text_color(s_type_layer, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  text_layer_set_font(s_type_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_type_layer, GTextAlignmentCenter);
  snprintf(s_type_buf, sizeof(s_type_buf), "[ %s ]", DIE_NAMES[s_die_type]);
  text_layer_set_text(s_type_layer, s_type_buf);
  layer_add_child(root, text_layer_get_layer(s_type_layer));

  // Die visual
  s_die_layer = layer_create(GRect(0, y_offset + 24, bounds.size.w, 100));
  layer_set_update_proc(s_die_layer, die_layer_update);
  layer_add_child(root, s_die_layer);

  // Combo streak indicator (positioned to right of die area)
  s_combo_layer = text_layer_create(GRect(bounds.size.w - 50, y_offset + 24, 48, 24));
  text_layer_set_background_color(s_combo_layer, GColorClear);
  text_layer_set_text_color(s_combo_layer, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
  text_layer_set_font(s_combo_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_combo_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_combo_layer));

  // Result text (for non-D6 big number display beneath die)
  s_result_layer = text_layer_create(GRect(0, y_offset + 120, bounds.size.w, 24));
  text_layer_set_background_color(s_result_layer, GColorClear);
  text_layer_set_text_color(s_result_layer, GColorWhite);
  text_layer_set_font(s_result_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_result_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_result_layer));

  // Stats layer (shown on long-press Down)
  s_stats_layer = text_layer_create(GRect(4, y_offset + 100, bounds.size.w - 8, 40));
  text_layer_set_background_color(s_stats_layer, GColorClear);
  text_layer_set_text_color(s_stats_layer, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
  text_layer_set_font(s_stats_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_stats_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_stats_layer, GTextOverflowModeWordWrap);
  layer_add_child(root, text_layer_get_layer(s_stats_layer));

  // History
  s_history_layer = text_layer_create(GRect(0, bounds.size.h - 20, bounds.size.w, 20));
  text_layer_set_background_color(s_history_layer, GColorClear);
  text_layer_set_text_color(s_history_layer, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
  text_layer_set_font(s_history_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_history_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_history_layer));

  // Show stats if persisted preference says so
  if (s_stats_visible) {
    show_stats();
  }

  // First-launch overlay
  if (!persist_exists(PERSIST_FIRST_LAUNCH)) {
    show_first_launch_overlay(window);
  }
}

static void window_unload(Window *window) {
  status_bar_layer_destroy(s_status_bar);
  text_layer_destroy(s_type_layer);
  text_layer_destroy(s_result_layer);
  text_layer_destroy(s_history_layer);
  text_layer_destroy(s_combo_layer);
  text_layer_destroy(s_stats_layer);
  layer_destroy(s_die_layer);
  if (s_overlay_layer) {
    text_layer_destroy(s_overlay_layer);
    s_overlay_layer = NULL;
  }
}

static void init(void) {
  srand(time(NULL));

  // Restore persisted die type
  if (persist_exists(PERSIST_DIE_TYPE)) {
    s_die_type = (DieType)persist_read_int(PERSIST_DIE_TYPE);
    if (s_die_type >= DIE_COUNT) s_die_type = DIE_D6;
  }

  // Load statistics
  load_stats();

  // BT connection handler
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bt_handler,
  });

  // Restore persisted show-stats preference
  if (persist_exists(PERSIST_SHOW_STATS)) {
    s_stats_visible = persist_read_bool(PERSIST_SHOW_STATS);
  }

  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  accel_tap_service_subscribe(tap_handler);

  // Set up AppMessage for phone config
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_open(128, 128);
}

static void deinit(void) {
  // Cancel any pending spin timer
  if (s_spin_timer) {
    app_timer_cancel(s_spin_timer);
    s_spin_timer = NULL;
  }
  if (s_overlay_timer) {
    app_timer_cancel(s_overlay_timer);
    s_overlay_timer = NULL;
  }

  // Persist die type
  persist_write_int(PERSIST_DIE_TYPE, (int)s_die_type);

  // Save statistics
  save_stats();

  connection_service_unsubscribe();
  accel_tap_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
