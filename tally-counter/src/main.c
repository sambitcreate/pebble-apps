#include <pebble.h>
#include "../../shared/pebble_pastel.h"

// ── Storage keys ──
#define STORAGE_KEY_COUNT       1
#define STORAGE_KEY_GOAL        2
#define STORAGE_KEY_HISTORY     3   // 7 ints (today + 6 previous days)
#define STORAGE_KEY_DAY         4   // day-of-year when last saved
#define STORAGE_KEY_CUSTOM_GOAL 5
#define STORAGE_KEY_STEP_SEL    6
#define STORAGE_KEY_STEP_UP     7

// ── AppMessage keys (must match messageKeys in package.json) ──
#define MSG_KEY_GOAL        0
#define MSG_KEY_STEP_SELECT 1
#define MSG_KEY_STEP_UP     2

// ── Goal presets (on-watch fallback) ──
static const int GOAL_OPTIONS[] = {0, 10, 25, 50, 100, 250, 500, 1000};
#define NUM_GOAL_OPTIONS 8

// ── History ──
#define HISTORY_DAYS 7

static PastelTheme s_theme;
static int s_theme_id = THEME_WARM_SUNSET;

// ── UI layers ──
static Window *s_window;
static StatusBarLayer *s_status_bar;
static Layer *s_canvas_layer;
static TextLayer *s_count_layer;
static TextLayer *s_circle_label_layer;   // "x1", "x2" etc
static TextLayer *s_goal_layer;           // "Goal: 100"
static TextLayer *s_yesterday_layer;      // "Yesterday: 42"
static TextLayer *s_hint_layer;
static TextLayer *s_overlay_layer;        // first-launch overlay

// ── State ──
static int s_count = 0;
static int s_goal_index = 0;              // index into GOAL_OPTIONS
static int s_custom_goal = 0;             // custom goal from phone (0 = use presets)
static int s_step_select = 1;             // select button increment
static int s_step_up = 5;                 // up button increment
static int s_history[HISTORY_DAYS];       // [0]=today, [1]=yesterday, ...
static int s_last_day = -1;              // day-of-year last saved
static bool s_first_launch = false;
static bool s_show_overlay = false;
static bool s_flash_active = false;

static char s_count_buf[12];
static char s_circle_buf[12];
static char s_goal_buf[20];
static char s_yesterday_buf[24];
static char s_hint_buf[32];

// ── BT state ──
static bool s_bt_connected = true;

// ── Flash timer ──
static AppTimer *s_flash_timer = NULL;

// ── Forward declarations ──
static void update_display(void);
static void save_all(void);

// ── Goal helper: custom goal takes priority over presets ──
static int get_goal(void) {
  if (s_custom_goal > 0) return s_custom_goal;
  return GOAL_OPTIONS[s_goal_index];
}

// ──────────────────────────────────────────────
// Persistence
// ──────────────────────────────────────────────

static int current_day_of_year(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  return t->tm_yday;
}

static void load_data(void) {
  // Count
  if (persist_exists(STORAGE_KEY_COUNT)) {
    s_count = persist_read_int(STORAGE_KEY_COUNT);
  } else {
    s_count = 0;
    s_first_launch = true;
  }

  // Goal
  if (persist_exists(STORAGE_KEY_GOAL)) {
    s_goal_index = persist_read_int(STORAGE_KEY_GOAL);
    if (s_goal_index < 0 || s_goal_index >= NUM_GOAL_OPTIONS) {
      s_goal_index = 0;
    }
  }

  // Custom goal from phone config
  if (persist_exists(STORAGE_KEY_CUSTOM_GOAL)) {
    s_custom_goal = persist_read_int(STORAGE_KEY_CUSTOM_GOAL);
  }

  // Step increments from phone config
  if (persist_exists(STORAGE_KEY_STEP_SEL)) {
    s_step_select = persist_read_int(STORAGE_KEY_STEP_SEL);
    if (s_step_select < 1) s_step_select = 1;
  }
  if (persist_exists(STORAGE_KEY_STEP_UP)) {
    s_step_up = persist_read_int(STORAGE_KEY_STEP_UP);
    if (s_step_up < 1) s_step_up = 1;
  }

  // History
  if (persist_exists(STORAGE_KEY_HISTORY)) {
    persist_read_data(STORAGE_KEY_HISTORY, s_history, sizeof(s_history));
  } else {
    memset(s_history, 0, sizeof(s_history));
  }

  // Day tracking — shift history on new day
  if (persist_exists(STORAGE_KEY_DAY)) {
    s_last_day = persist_read_int(STORAGE_KEY_DAY);
  }

  // Load theme
  if (persist_exists(PASTEL_STORAGE_KEY_THEME)) {
    s_theme_id = persist_read_int(PASTEL_STORAGE_KEY_THEME);
  }
  s_theme = pastel_get_theme(s_theme_id);

  int today = current_day_of_year();
  if (s_last_day >= 0 && s_last_day != today) {
    // Calculate how many days have passed (handle year wrap)
    int days_passed = today - s_last_day;
    if (days_passed < 0) days_passed += 365;
    if (days_passed > HISTORY_DAYS) days_passed = HISTORY_DAYS;

    // Shift history forward
    for (int i = HISTORY_DAYS - 1; i >= days_passed; i--) {
      s_history[i] = s_history[i - days_passed];
    }
    // Fill gap days with 0 (except today's slot)
    for (int i = 1; i < days_passed && i < HISTORY_DAYS; i++) {
      s_history[i] = 0;
    }
    // Today's count stored yesterday; now stored in s_history[0] already shifted
    // Start fresh today
    s_history[0] = s_count;
    // Actually reset today's running count? No — keep accumulating.
    // s_history[0] will be updated on every save with current count.
  }

  s_last_day = today;
}

static void save_all(void) {
  persist_write_int(STORAGE_KEY_COUNT, s_count);
  persist_write_int(STORAGE_KEY_GOAL, s_goal_index);
  persist_write_int(STORAGE_KEY_CUSTOM_GOAL, s_custom_goal);
  persist_write_int(STORAGE_KEY_STEP_SEL, s_step_select);
  persist_write_int(STORAGE_KEY_STEP_UP, s_step_up);

  // Update today's history slot
  s_history[0] = s_count;
  persist_write_data(STORAGE_KEY_HISTORY, s_history, sizeof(s_history));
  persist_write_int(STORAGE_KEY_DAY, current_day_of_year());
}

// ──────────────────────────────────────────────
// Drawing
// ──────────────────────────────────────────────

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  // Shift center down slightly to account for status bar
  center.y += 4;

  int radius = 55;
  GRect arc_rect = GRect(center.x - radius, center.y - radius,
                         radius * 2, radius * 2);

  // ── Background ring ──
  graphics_context_set_stroke_color(ctx, s_theme.muted);
  graphics_context_set_stroke_width(ctx, 4);
  graphics_draw_arc(ctx, arc_rect, GOvalScaleModeFitCircle,
                    0, TRIG_MAX_ANGLE);

  // ── Progress arc ──
  int angle = (s_count % 100) * 360 / 100;
  GColor arc_color;
  if (s_flash_active) {
    arc_color = s_theme.accent;
  } else {
    arc_color = s_theme.highlight;
  }
  graphics_context_set_stroke_color(ctx, arc_color);
  graphics_context_set_stroke_width(ctx, 4);
  if (angle > 0) {
    graphics_draw_arc(ctx, arc_rect, GOvalScaleModeFitCircle,
                      0, DEG_TO_TRIGANGLE(angle));
  }

  // ── Goal marker (small tick on the ring) ──
  if (get_goal() > 0) {
    int goal_val = get_goal();
    int goal_angle_deg = (goal_val % 100) * 360 / 100;
    if (goal_angle_deg == 0 && goal_val > 0) goal_angle_deg = 360;
    int32_t goal_trig = DEG_TO_TRIGANGLE(goal_angle_deg);

    // Draw a small dot on the ring at the goal position
    int dot_r = radius;
    int dot_x = center.x + (dot_r * sin_lookup(goal_trig)) / TRIG_MAX_RATIO;
    int dot_y = center.y - (dot_r * cos_lookup(goal_trig)) / TRIG_MAX_RATIO;

    graphics_context_set_fill_color(ctx, s_theme.highlight);
    graphics_fill_circle(ctx, GPoint(dot_x, dot_y), 3);
  }
}

// ──────────────────────────────────────────────
// Flash effect for goal celebration
// ──────────────────────────────────────────────

static void flash_timer_callback(void *data) {
  s_flash_active = false;
  s_flash_timer = NULL;
  layer_mark_dirty(s_canvas_layer);
}

static void celebrate_goal(void) {
  // Vibration pattern
  static const uint32_t segs[] = {80, 50, 80, 50, 150};
  VibePattern pat = { .durations = segs, .num_segments = 5 };
  vibes_enqueue_custom_pattern(pat);

  // Flash effect
  s_flash_active = true;
  layer_mark_dirty(s_canvas_layer);
  if (s_flash_timer) {
    app_timer_cancel(s_flash_timer);
  }
  s_flash_timer = app_timer_register(600, flash_timer_callback, NULL);
}

// ──────────────────────────────────────────────
// Display
// ──────────────────────────────────────────────

static void update_display(void) {
  // Count text
  snprintf(s_count_buf, sizeof(s_count_buf), "%d", s_count);
  text_layer_set_text(s_count_layer, s_count_buf);

  // Circle multiplier label
  int circles = s_count / 100;
  if (circles > 0) {
    snprintf(s_circle_buf, sizeof(s_circle_buf), "x%d", circles);
  } else {
    s_circle_buf[0] = '\0';
  }
  text_layer_set_text(s_circle_label_layer, s_circle_buf);

  // Goal text
  if (get_goal() > 0) {
    snprintf(s_goal_buf, sizeof(s_goal_buf), "Goal: %d", get_goal());
  } else {
    snprintf(s_goal_buf, sizeof(s_goal_buf), "No Goal");
  }
  text_layer_set_text(s_goal_layer, s_goal_buf);

  // Yesterday's count
  if (s_history[1] > 0) {
    snprintf(s_yesterday_buf, sizeof(s_yesterday_buf), "Yesterday: %d", s_history[1]);
  } else {
    snprintf(s_yesterday_buf, sizeof(s_yesterday_buf), " ");
  }
  text_layer_set_text(s_yesterday_layer, s_yesterday_buf);

  // Hint text with current step values
  snprintf(s_hint_buf, sizeof(s_hint_buf), "Sel:+%d  Up:+%d  Dn:-1",
           s_step_select, s_step_up);
  text_layer_set_text(s_hint_layer, s_hint_buf);

  // Redraw canvas
  layer_mark_dirty(s_canvas_layer);
}

// ──────────────────────────────────────────────
// Overlay (first launch)
// ──────────────────────────────────────────────

static void dismiss_overlay(void) {
  if (s_show_overlay && s_overlay_layer) {
    s_show_overlay = false;
    layer_set_hidden(text_layer_get_layer(s_overlay_layer), true);
  }
}

// ──────────────────────────────────────────────
// Button handlers
// ──────────────────────────────────────────────

static void check_goal(void) {
  int goal = get_goal();
  if (goal > 0 && s_count == goal) {
    celebrate_goal();
  }
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  if (s_show_overlay) { dismiss_overlay(); return; }
  s_count += s_step_select;
  update_display();
  save_all();
  check_goal();

  // Subtle haptic
  static const uint32_t segs[] = {30};
  VibePattern pat = { .durations = segs, .num_segments = 1 };
  vibes_enqueue_custom_pattern(pat);
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  if (s_show_overlay) { dismiss_overlay(); return; }
  int old = s_count;
  s_count += s_step_up;
  update_display();
  save_all();

  // Check if we crossed the goal
  int goal = get_goal();
  if (goal > 0 && old < goal && s_count >= goal) {
    celebrate_goal();
  } else {
    vibes_short_pulse();
  }
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  if (s_show_overlay) { dismiss_overlay(); return; }
  if (s_count > 0) {
    s_count--;
    update_display();
    save_all();
  }
}

static void select_long_click(ClickRecognizerRef recognizer, void *context) {
  if (s_show_overlay) { dismiss_overlay(); return; }
  s_count = 0;
  update_display();
  save_all();

  // Double vibrate for reset
  static const uint32_t segs[] = {100, 80, 100};
  VibePattern pat = { .durations = segs, .num_segments = 3 };
  vibes_enqueue_custom_pattern(pat);
}

static void up_long_click(ClickRecognizerRef recognizer, void *context) {
  if (s_show_overlay) { dismiss_overlay(); return; }

  // Cycle through goal options
  s_goal_index = (s_goal_index + 1) % NUM_GOAL_OPTIONS;
  update_display();
  save_all();

  // Confirm vibration
  static const uint32_t segs[] = {40};
  VibePattern pat = { .durations = segs, .num_segments = 1 };
  vibes_enqueue_custom_pattern(pat);
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 1000, select_long_click, NULL);
  window_long_click_subscribe(BUTTON_ID_UP, 700, up_long_click, NULL);
}

// ──────────────────────────────────────────────
// Bluetooth
// ──────────────────────────────────────────────

static void bt_handler(bool connected) {
  s_bt_connected = connected;
  if (!connected) {
    // Vibrate to alert disconnect
    static const uint32_t segs[] = {200, 100, 200, 100, 200};
    VibePattern pat = { .durations = segs, .num_segments = 5 };
    vibes_enqueue_custom_pattern(pat);
  }
}

// ──────────────────────────────────────────────
// Window load / unload
// ──────────────────────────────────────────────

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, GColorBlack);

  // ── Status bar with dotted separator ──
  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, GColorBlack, s_theme.accent);
  status_bar_layer_set_separator_mode(s_status_bar,
    StatusBarLayerSeparatorModeDotted);
  layer_add_child(root, status_bar_layer_get_layer(s_status_bar));

  // Content area shifted down by STATUS_BAR_LAYER_HEIGHT (16px)
  int yoff = STATUS_BAR_LAYER_HEIGHT;

  // ── Canvas for circular arc ──
  s_canvas_layer = layer_create(GRect(0, yoff, bounds.size.w,
                                      bounds.size.h - yoff));
  layer_set_update_proc(s_canvas_layer, canvas_update);
  layer_add_child(root, s_canvas_layer);

  // ── Big count (centered in arc) ──
  // Arc center is at roughly (w/2, (h-yoff)/2 + 4) within canvas
  int canvas_h = bounds.size.h - yoff;
  int count_y = (canvas_h / 2) + 4 - 28;  // ~28 is half of BITHAM_42 height
  s_count_layer = text_layer_create(GRect(0, count_y,
                                          bounds.size.w, 50));
  text_layer_set_background_color(s_count_layer, GColorClear);
  text_layer_set_text_color(s_count_layer, s_theme.primary);
  text_layer_set_font(s_count_layer,
    fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_count_layer, GTextAlignmentCenter);
  layer_add_child(s_canvas_layer, text_layer_get_layer(s_count_layer));

  // ── Circle multiplier label (below count) ──
  s_circle_label_layer = text_layer_create(GRect(0, count_y + 46,
                                                  bounds.size.w, 20));
  text_layer_set_background_color(s_circle_label_layer, GColorClear);
  text_layer_set_text_color(s_circle_label_layer, s_theme.accent);
  text_layer_set_font(s_circle_label_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_circle_label_layer, GTextAlignmentCenter);
  layer_add_child(s_canvas_layer, text_layer_get_layer(s_circle_label_layer));

  // ── Goal label (top area, inside canvas) ──
  s_goal_layer = text_layer_create(GRect(0, 2, bounds.size.w, 18));
  text_layer_set_background_color(s_goal_layer, GColorClear);
  text_layer_set_text_color(s_goal_layer, s_theme.muted);
  text_layer_set_font(s_goal_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_goal_layer, GTextAlignmentCenter);
  layer_add_child(s_canvas_layer, text_layer_get_layer(s_goal_layer));

  // ── Yesterday label (bottom area) ──
  s_yesterday_layer = text_layer_create(GRect(0, canvas_h - 34,
                                               bounds.size.w, 18));
  text_layer_set_background_color(s_yesterday_layer, GColorClear);
  text_layer_set_text_color(s_yesterday_layer, s_theme.muted);
  text_layer_set_font(s_yesterday_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_yesterday_layer, GTextAlignmentCenter);
  layer_add_child(s_canvas_layer, text_layer_get_layer(s_yesterday_layer));

  // ── Hints (bottom) ──
  s_hint_layer = text_layer_create(GRect(0, canvas_h - 20,
                                          bounds.size.w, 20));
  text_layer_set_background_color(s_hint_layer, GColorClear);
  text_layer_set_text_color(s_hint_layer, s_theme.muted);
  text_layer_set_font(s_hint_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_hint_layer, GTextAlignmentCenter);
  // Hint text set dynamically in update_display()
  layer_add_child(s_canvas_layer, text_layer_get_layer(s_hint_layer));

  // ── First-launch overlay ──
  s_overlay_layer = text_layer_create(GRect(8, 20,
                                            bounds.size.w - 16, canvas_h - 30));
  text_layer_set_background_color(s_overlay_layer,
    PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack));
  text_layer_set_text_color(s_overlay_layer, GColorWhite);
  text_layer_set_font(s_overlay_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_overlay_layer, GTextAlignmentCenter);
  text_layer_set_text(s_overlay_layer,
    "\nTally Counter\n\n"
    "Sel: +1\n"
    "Up: +5\n"
    "Down: -1\n"
    "Hold Sel: Reset\n"
    "Hold Up: Set Goal\n\n"
    "Press any button...");
  text_layer_set_overflow_mode(s_overlay_layer, GTextOverflowModeWordWrap);
  layer_add_child(s_canvas_layer, text_layer_get_layer(s_overlay_layer));

  // Load data first so s_first_launch is set before overlay check
  load_data();

  if (s_first_launch) {
    s_show_overlay = true;
    layer_set_hidden(text_layer_get_layer(s_overlay_layer), false);
  } else {
    s_show_overlay = false;
    layer_set_hidden(text_layer_get_layer(s_overlay_layer), true);
  }

  update_display();
}

static void window_unload(Window *window) {
  text_layer_destroy(s_count_layer);
  text_layer_destroy(s_circle_label_layer);
  text_layer_destroy(s_goal_layer);
  text_layer_destroy(s_yesterday_layer);
  text_layer_destroy(s_hint_layer);
  text_layer_destroy(s_overlay_layer);
  layer_destroy(s_canvas_layer);
  status_bar_layer_destroy(s_status_bar);
}

// ──────────────────────────────────────────────
// AppMessage: receive config from phone
// ──────────────────────────────────────────────

static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *goal_t = dict_find(iter, MSG_KEY_GOAL);
  if (goal_t) {
    s_custom_goal = (int)goal_t->value->int32;
    if (s_custom_goal < 0) s_custom_goal = 0;
    persist_write_int(STORAGE_KEY_CUSTOM_GOAL, s_custom_goal);
  }

  Tuple *step_sel_t = dict_find(iter, MSG_KEY_STEP_SELECT);
  if (step_sel_t) {
    s_step_select = (int)step_sel_t->value->int32;
    if (s_step_select < 1) s_step_select = 1;
    persist_write_int(STORAGE_KEY_STEP_SEL, s_step_select);
  }

  Tuple *step_up_t = dict_find(iter, MSG_KEY_STEP_UP);
  if (step_up_t) {
    s_step_up = (int)step_up_t->value->int32;
    if (s_step_up < 1) s_step_up = 1;
    persist_write_int(STORAGE_KEY_STEP_UP, s_step_up);
  }

  Tuple *theme_t = dict_find(iter, PASTEL_MSG_KEY_THEME);
  if (theme_t) {
    s_theme_id = theme_t->value->int32;
    persist_write_int(PASTEL_STORAGE_KEY_THEME, s_theme_id);
    s_theme = pastel_get_theme(s_theme_id);
    // Re-apply theme colors
    text_layer_set_text_color(s_count_layer, s_theme.primary);
    text_layer_set_text_color(s_circle_label_layer, s_theme.accent);
    text_layer_set_text_color(s_goal_layer, s_theme.muted);
    text_layer_set_text_color(s_yesterday_layer, s_theme.muted);
    text_layer_set_text_color(s_hint_layer, s_theme.muted);
    status_bar_layer_set_colors(s_status_bar, GColorBlack, s_theme.accent);
  }

  update_display();
  save_all();
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", reason);
}

// ──────────────────────────────────────────────
// Init / deinit
// ──────────────────────────────────────────────

static void init(void) {
  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  // BT monitoring
  s_bt_connected = connection_service_peek_pebble_app_connection();
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bt_handler,
  });

  // Set up AppMessage for phone config
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_open(128, 64);

  window_stack_push(s_window, true);
}

static void deinit(void) {
  save_all();
  connection_service_unsubscribe();
  if (s_flash_timer) {
    app_timer_cancel(s_flash_timer);
    s_flash_timer = NULL;
  }
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
