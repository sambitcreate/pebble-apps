#include <pebble.h>
#include "../../shared/pebble_pastel.h"

static PastelTheme s_theme;
static int s_theme_id = THEME_WARM_SUNSET;

static Window *s_window;
static TextLayer *s_time_layer;
static TextLayer *s_phase_layer;
static TextLayer *s_count_layer;
static Layer *s_progress_layer;
static StatusBarLayer *s_status_bar;

// Instruction overlay layers
static Layer *s_overlay_layer;
static TextLayer *s_instr_title_layer;
static TextLayer *s_instr_body_layer;
static bool s_show_instructions = false;

typedef enum {
  PHASE_WORK,
  PHASE_SHORT_BREAK,
  PHASE_LONG_BREAK,
} Phase;

// Configurable durations (defaults; overridden by phone config)
static int WORK_SECONDS     = 25 * 60;
static int SHORT_BREAK_SEC  = 5 * 60;
static int LONG_BREAK_SEC   = 15 * 60;
static int POMOS_BEFORE_LONG = 4;

// AppMessage keys (must match messageKeys in package.json)
#define MSG_KEY_WORK_DURATION     0
#define MSG_KEY_SHORT_BREAK       1
#define MSG_KEY_LONG_BREAK        2
#define MSG_KEY_POMOS_BEFORE_LONG 3

// Persistent storage keys
#define STORAGE_PHASE     1
#define STORAGE_REMAINING 2
#define STORAGE_COUNT     3
#define STORAGE_RUNNING   4
#define STORAGE_FIRST_LAUNCH 100

// Persistent storage keys for config
#define STORAGE_CFG_WORK      10
#define STORAGE_CFG_SHORT     11
#define STORAGE_CFG_LONG      12
#define STORAGE_CFG_POMOS     13

// Session dot constants
#define MAX_SESSION_DOTS 12
#define DOT_RADIUS       4
#define DOT_SPACING      4

static Phase s_phase = PHASE_WORK;
static int s_seconds_left = 25 * 60;
static int s_phase_duration = 25 * 60;
static int s_pomo_count = 0;
static bool s_running = false;
static char s_time_buf[8];
static char s_count_buf[16];

// Bluetooth state
static bool s_bt_connected = true;

static int phase_duration(Phase p) {
  switch (p) {
    case PHASE_WORK:        return WORK_SECONDS;
    case PHASE_SHORT_BREAK: return SHORT_BREAK_SEC;
    case PHASE_LONG_BREAK:  return LONG_BREAK_SEC;
  }
  return WORK_SECONDS;
}

static const char *phase_name(Phase p) {
  switch (p) {
    case PHASE_WORK:        return "WORK";
    case PHASE_SHORT_BREAK: return "BREAK";
    case PHASE_LONG_BREAK:  return "LONG BREAK";
  }
  return "WORK";
}

static void update_display(void) {
  int mins = s_seconds_left / 60;
  int secs = s_seconds_left % 60;
  snprintf(s_time_buf, sizeof(s_time_buf), "%02d:%02d", mins, secs);
  text_layer_set_text(s_time_layer, s_time_buf);

  text_layer_set_text(s_phase_layer, phase_name(s_phase));
  text_layer_set_text_color(s_phase_layer, s_theme.accent);

  snprintf(s_count_buf, sizeof(s_count_buf), "Pomos: %d", s_pomo_count);
  text_layer_set_text(s_count_layer, s_count_buf);

  layer_mark_dirty(s_progress_layer);
}

static void next_phase(void) {
  if (s_phase == PHASE_WORK) {
    s_pomo_count++;
    if (s_pomo_count % POMOS_BEFORE_LONG == 0) {
      s_phase = PHASE_LONG_BREAK;
    } else {
      s_phase = PHASE_SHORT_BREAK;
    }
  } else {
    s_phase = PHASE_WORK;
  }

  s_phase_duration = phase_duration(s_phase);
  s_seconds_left = s_phase_duration;

  // Vibrate to signal transition
  if (s_phase == PHASE_WORK) {
    static const uint32_t segs[] = {200, 100, 200, 100, 200};
    VibePattern pat = { .durations = segs, .num_segments = 5 };
    vibes_enqueue_custom_pattern(pat);
  } else {
    static const uint32_t segs[] = {400, 200, 400};
    VibePattern pat = { .durations = segs, .num_segments = 3 };
    vibes_enqueue_custom_pattern(pat);
  }

  update_display();
}

static GColor get_phase_color(void) {
  return s_theme.highlight;
}

static void draw_session_dots(GContext *ctx, GRect bounds) {
  int total_dots = MAX_SESSION_DOTS;
  int filled = s_pomo_count % MAX_SESSION_DOTS;
  if (s_pomo_count > 0 && filled == 0 && s_pomo_count >= MAX_SESSION_DOTS) {
    filled = MAX_SESSION_DOTS;
  }

  int dot_diameter = DOT_RADIUS * 2;
  int total_width = total_dots * dot_diameter + (total_dots - 1) * DOT_SPACING;
  int start_x = (bounds.size.w - total_width) / 2;
  int dot_y = bounds.size.h - DOT_RADIUS - 6;

  for (int i = 0; i < total_dots; i++) {
    int cx = start_x + i * (dot_diameter + DOT_SPACING) + DOT_RADIUS;
    GPoint center = GPoint(cx, dot_y);

    if (i < filled) {
      // Filled dot
      graphics_context_set_fill_color(ctx, s_theme.accent);
      graphics_fill_circle(ctx, center, DOT_RADIUS);
    } else {
      // Hollow dot
      graphics_context_set_stroke_color(ctx, s_theme.muted);
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_circle(ctx, center, DOT_RADIUS);
    }
  }
}

static void progress_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // --- Circular countdown arc ---
  int remaining = s_seconds_left;
  int total = s_phase_duration;
  int angle = remaining * 360 / total;
  GPoint center = grect_center_point(&bounds);
  // Shift center up a bit to make room for dots at bottom
  center.y -= 10;
  GRect arc_rect = GRect(center.x - 50, center.y - 50, 100, 100);

  // Background track
  graphics_context_set_stroke_color(ctx, s_theme.muted);
  graphics_context_set_stroke_width(ctx, 6);
  graphics_draw_arc(ctx, arc_rect, GOvalScaleModeFitCircle, 0, TRIG_MAX_ANGLE);

  // Countdown arc (draws from 12 o'clock position clockwise)
  if (angle > 0) {
    GColor arc_color = get_phase_color();
    graphics_context_set_stroke_color(ctx, arc_color);
    graphics_context_set_stroke_width(ctx, 6);
    graphics_draw_arc(ctx, arc_rect, GOvalScaleModeFitCircle, 0, DEG_TO_TRIGANGLE(angle));
  }

  // --- Session dots at the bottom ---
  draw_session_dots(ctx, bounds);
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  if (!s_running) return;

  if (s_seconds_left > 0) {
    s_seconds_left--;
    update_display();
  } else {
    next_phase();
  }
}

// --- Bluetooth handler ---
static void bt_handler(bool connected) {
  s_bt_connected = connected;
  if (!connected) {
    vibes_double_pulse();
  }
}

// --- Instruction overlay ---
static void overlay_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  // Semi-transparent dark background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
}

static void dismiss_instructions(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  if (s_show_instructions) {
    s_show_instructions = false;
    // Remove overlay layers
    layer_remove_from_parent(s_overlay_layer);
    layer_remove_from_parent(text_layer_get_layer(s_instr_title_layer));
    layer_remove_from_parent(text_layer_get_layer(s_instr_body_layer));
    // Mark first launch done
    persist_write_int(STORAGE_FIRST_LAUNCH, 1);
  }
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  if (s_show_instructions) {
    dismiss_instructions(recognizer, context);
    return;
  }
  s_running = !s_running;
  update_display();
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  if (s_show_instructions) {
    dismiss_instructions(recognizer, context);
    return;
  }
  // Reset current phase
  s_seconds_left = s_phase_duration;
  s_running = false;
  update_display();
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  if (s_show_instructions) {
    dismiss_instructions(recognizer, context);
    return;
  }
  // Skip to next phase
  next_phase();
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

static void show_instruction_overlay(Layer *root, GRect bounds) {
  s_show_instructions = true;

  // Full-screen dark overlay
  s_overlay_layer = layer_create(bounds);
  layer_set_update_proc(s_overlay_layer, overlay_update);
  layer_add_child(root, s_overlay_layer);

  // Title
  s_instr_title_layer = text_layer_create(GRect(10, bounds.size.h / 4, bounds.size.w - 20, 30));
  text_layer_set_background_color(s_instr_title_layer, GColorClear);
  text_layer_set_text_color(s_instr_title_layer, GColorWhite);
  text_layer_set_font(s_instr_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_instr_title_layer, GTextAlignmentCenter);
  text_layer_set_text(s_instr_title_layer, "Pomodoro Timer");
  layer_add_child(root, text_layer_get_layer(s_instr_title_layer));

  // Instructions body
  s_instr_body_layer = text_layer_create(GRect(10, bounds.size.h / 4 + 35, bounds.size.w - 20, 90));
  text_layer_set_background_color(s_instr_body_layer, GColorClear);
  text_layer_set_text_color(s_instr_body_layer, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  text_layer_set_font(s_instr_body_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_instr_body_layer, GTextAlignmentCenter);
  text_layer_set_text(s_instr_body_layer,
    "UP: Reset phase\n"
    "SELECT: Start/Pause\n"
    "DOWN: Skip phase\n\n"
    "Press any button...");
  layer_add_child(root, text_layer_get_layer(s_instr_body_layer));
}

// Load persisted config values
static void load_config(void) {
  if (persist_exists(STORAGE_CFG_WORK)) {
    WORK_SECONDS = persist_read_int(STORAGE_CFG_WORK);
  }
  if (persist_exists(STORAGE_CFG_SHORT)) {
    SHORT_BREAK_SEC = persist_read_int(STORAGE_CFG_SHORT);
  }
  if (persist_exists(STORAGE_CFG_LONG)) {
    LONG_BREAK_SEC = persist_read_int(STORAGE_CFG_LONG);
  }
  if (persist_exists(STORAGE_CFG_POMOS)) {
    POMOS_BEFORE_LONG = persist_read_int(STORAGE_CFG_POMOS);
  }
}

// AppMessage: receive config from phone
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;

  t = dict_find(iter, MSG_KEY_WORK_DURATION);
  if (t) {
    WORK_SECONDS = (int)t->value->int32 * 60;
    persist_write_int(STORAGE_CFG_WORK, WORK_SECONDS);
  }

  t = dict_find(iter, MSG_KEY_SHORT_BREAK);
  if (t) {
    SHORT_BREAK_SEC = (int)t->value->int32 * 60;
    persist_write_int(STORAGE_CFG_SHORT, SHORT_BREAK_SEC);
  }

  t = dict_find(iter, MSG_KEY_LONG_BREAK);
  if (t) {
    LONG_BREAK_SEC = (int)t->value->int32 * 60;
    persist_write_int(STORAGE_CFG_LONG, LONG_BREAK_SEC);
  }

  t = dict_find(iter, MSG_KEY_POMOS_BEFORE_LONG);
  if (t) {
    POMOS_BEFORE_LONG = (int)t->value->int32;
    persist_write_int(STORAGE_CFG_POMOS, POMOS_BEFORE_LONG);
  }

  Tuple *theme_t = dict_find(iter, PASTEL_MSG_KEY_THEME);
  if (theme_t) {
    s_theme_id = theme_t->value->int32;
    persist_write_int(PASTEL_STORAGE_KEY_THEME, s_theme_id);
    s_theme = pastel_get_theme(s_theme_id);
    // Re-apply theme colors
    text_layer_set_text_color(s_phase_layer, s_theme.accent);
    text_layer_set_text_color(s_time_layer, s_theme.primary);
    text_layer_set_text_color(s_count_layer, s_theme.secondary);
  }

  // Update current phase duration in case it changed
  s_phase_duration = phase_duration(s_phase);
  // If the timer is not running, reset seconds_left to match new duration
  if (!s_running) {
    s_seconds_left = s_phase_duration;
  }
  update_display();
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", reason);
}

static void restore_state(void) {
  // Load config first so phase_duration() uses correct values
  load_config();

  // Load theme
  if (persist_exists(PASTEL_STORAGE_KEY_THEME)) {
    s_theme_id = persist_read_int(PASTEL_STORAGE_KEY_THEME);
  }
  s_theme = pastel_get_theme(s_theme_id);

  if (persist_exists(STORAGE_PHASE)) {
    s_phase = (Phase)persist_read_int(STORAGE_PHASE);
  }
  if (persist_exists(STORAGE_REMAINING)) {
    s_seconds_left = persist_read_int(STORAGE_REMAINING);
  }
  if (persist_exists(STORAGE_COUNT)) {
    s_pomo_count = persist_read_int(STORAGE_COUNT);
  }
  if (persist_exists(STORAGE_RUNNING)) {
    s_running = persist_read_bool(STORAGE_RUNNING);
  }
  s_phase_duration = phase_duration(s_phase);
  // Clamp seconds_left to valid range
  if (s_seconds_left < 0 || s_seconds_left > s_phase_duration) {
    s_seconds_left = s_phase_duration;
  }
}

static void save_state(void) {
  persist_write_int(STORAGE_PHASE, (int)s_phase);
  persist_write_int(STORAGE_REMAINING, s_seconds_left);
  persist_write_int(STORAGE_COUNT, s_pomo_count);
  persist_write_bool(STORAGE_RUNNING, s_running);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, GColorBlack);

  // Restore persistent state
  restore_state();

  // Status bar at the top
  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, GColorBlack, GColorWhite);
  status_bar_layer_set_separator_mode(s_status_bar, StatusBarLayerSeparatorModeDotted);
  layer_add_child(root, status_bar_layer_get_layer(s_status_bar));

  // Offset for content below status bar
  int y_offset = STATUS_BAR_LAYER_HEIGHT;

  // Compute arc center for positioning text layers
  GRect content_bounds = GRect(0, y_offset, bounds.size.w, bounds.size.h - y_offset);
  GPoint center = grect_center_point(&content_bounds);
  center.y -= 10; // Match the arc center offset in progress_update

  // Phase label - above the arc
  s_phase_layer = text_layer_create(GRect(0, center.y - 50 - 30, bounds.size.w, 30));
  text_layer_set_background_color(s_phase_layer, GColorClear);
  text_layer_set_text_color(s_phase_layer, s_theme.accent);
  text_layer_set_font(s_phase_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_phase_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_phase_layer));

  // Time display - centered inside the arc
  s_time_layer = text_layer_create(GRect(0, center.y - 24, bounds.size.w, 50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, s_theme.primary);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  // Pomo count - below the arc
  s_count_layer = text_layer_create(GRect(0, center.y + 50 + 4, bounds.size.w, 24));
  text_layer_set_background_color(s_count_layer, GColorClear);
  text_layer_set_text_color(s_count_layer, s_theme.secondary);
  text_layer_set_font(s_count_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_count_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_count_layer));

  // Progress layer (arc + session dots) - covers area below status bar
  s_progress_layer = layer_create(GRect(0, y_offset, bounds.size.w, bounds.size.h - y_offset));
  layer_set_update_proc(s_progress_layer, progress_update);
  layer_add_child(root, s_progress_layer);

  update_display();

  // Show instruction overlay on first launch
  if (!persist_exists(STORAGE_FIRST_LAUNCH)) {
    show_instruction_overlay(root, bounds);
  }
}

static void window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_phase_layer);
  text_layer_destroy(s_count_layer);
  layer_destroy(s_progress_layer);
  status_bar_layer_destroy(s_status_bar);

  if (s_show_instructions) {
    text_layer_destroy(s_instr_title_layer);
    text_layer_destroy(s_instr_body_layer);
    layer_destroy(s_overlay_layer);
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
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);

  // Subscribe to Bluetooth connection events
  s_bt_connected = connection_service_peek_pebble_app_connection();
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bt_handler,
  });

  // Set up AppMessage for phone config
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_open(128, 128);
}

static void deinit(void) {
  // Save state before exit
  save_state();

  connection_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
