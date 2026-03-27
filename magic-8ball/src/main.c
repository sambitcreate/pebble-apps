#include <pebble.h>
#include "../../shared/pebble_pastel.h"

static PastelTheme s_theme;
static int s_theme_id = THEME_WARM_SUNSET;

// AppMessage keys — must match messageKeys in package.json
#define MSG_KEY_VIBRATION 0
#define MSG_KEY_ANIM_SPEED 1

// Persist storage keys
#define STORAGE_KEY_VIBRATION 10
#define STORAGE_KEY_ANIM_SPEED 11

static Window *s_window;
static Layer *s_canvas;
static TextLayer *s_answer_layer;
static TextLayer *s_prompt_layer;
static StatusBarLayer *s_status_bar;

// First-launch overlay
static Layer *s_overlay_layer;
static TextLayer *s_overlay_text;
static bool s_first_launch = true;

// Configuration
static bool s_vibration_enabled = true;   // default: on
static int s_anim_frame_ms = 40;          // default: Normal (40ms)

// Animation state
static AppTimer *s_anim_timer;
static int s_scale_pct = 0;       // 0..100 triangle grow percentage
static bool s_animating = false;

// History dots: last 5 answer indices (-1 = unused)
#define HISTORY_MAX 5
static int s_history[HISTORY_MAX] = {-1, -1, -1, -1, -1};
static int s_history_count = 0;

static const char *ANSWERS[] = {
  "It is\ncertain",
  "It is\ndecidedly so",
  "Without\na doubt",
  "Yes\ndefinitely",
  "You may\nrely on it",
  "As I\nsee it, yes",
  "Most\nlikely",
  "Outlook\ngood",
  "Yes",
  "Signs\npoint to yes",
  "Reply hazy\ntry again",
  "Ask again\nlater",
  "Better not\ntell you now",
  "Cannot\npredict now",
  "Concentrate\nand ask again",
  "Don't\ncount on it",
  "My reply\nis no",
  "My sources\nsay no",
  "Outlook\nnot so good",
  "Very\ndoubtful",
};
#define NUM_ANSWERS 20

static int s_current = 0;
static bool s_revealed = false;

// Forward declarations
static void dismiss_overlay(void);

// Answer category: 0-9 positive, 10-14 neutral, 15-19 negative
typedef enum {
  CATEGORY_POSITIVE,
  CATEGORY_NEUTRAL,
  CATEGORY_NEGATIVE,
} AnswerCategory;

static AnswerCategory get_category(int index) {
  if (index < 10) return CATEGORY_POSITIVE;
  if (index < 15) return CATEGORY_NEUTRAL;
  return CATEGORY_NEGATIVE;
}

static GColor category_color(AnswerCategory cat) {
  switch (cat) {
    case CATEGORY_POSITIVE: return s_theme.accent;
    case CATEGORY_NEUTRAL:  return s_theme.secondary;
    case CATEGORY_NEGATIVE: return s_theme.highlight;
  }
  return s_theme.primary;
}

// Push answer index into history ring
static void history_push(int answer_idx) {
  // Shift everything right
  for (int i = HISTORY_MAX - 1; i > 0; i--) {
    s_history[i] = s_history[i - 1];
  }
  s_history[0] = answer_idx;
  if (s_history_count < HISTORY_MAX) s_history_count++;
}

// --- Animation timer callback ---
static void anim_timer_callback(void *data) {
  s_anim_timer = NULL;
  if (!s_animating) return;

  s_scale_pct += 8;  // ~12-13 frames over ~500ms at 40ms interval
  if (s_scale_pct >= 100) {
    s_scale_pct = 100;
    s_animating = false;
    // Show answer text now that triangle is fully grown
    text_layer_set_text(s_answer_layer, ANSWERS[s_current]);
  } else {
    // Ease-out: decelerate towards end
    s_anim_timer = app_timer_register(s_anim_frame_ms, anim_timer_callback, NULL);
  }
  layer_mark_dirty(s_canvas);
}

static void start_reveal_animation(void) {
  s_scale_pct = 0;
  s_animating = true;
  // Hide text during grow animation
  text_layer_set_text(s_answer_layer, "");
  s_anim_timer = app_timer_register(s_anim_frame_ms, anim_timer_callback, NULL);
}

// Ease-out function: fast start, slow end
static int ease_out(int pct) {
  // quadratic ease-out: 1 - (1-t)^2 mapped to 0..100
  int inv = 100 - pct;
  return 100 - (inv * inv / 100);
}

static void reveal_answer(void) {
  // Dismiss first-launch overlay if showing
  if (s_first_launch) {
    dismiss_overlay();
  }

  s_current = rand() % NUM_ANSWERS;
  s_revealed = true;

  // Push to history
  history_push(s_current);

  text_layer_set_text(s_prompt_layer, "");
  layer_mark_dirty(s_canvas);

  // Start grow animation
  start_reveal_animation();

  // Vibrate (if enabled)
  if (s_vibration_enabled) {
    static const uint32_t segments[] = {80, 40, 80};
    VibePattern pat = { .durations = segments, .num_segments = 3 };
    vibes_enqueue_custom_pattern(pat);
  }
}

static void reset_ball(void) {
  s_revealed = false;
  s_animating = false;
  s_scale_pct = 0;
  if (s_anim_timer) {
    app_timer_cancel(s_anim_timer);
    s_anim_timer = NULL;
  }
  text_layer_set_text(s_answer_layer, "");
  text_layer_set_text(s_prompt_layer, "Shake me\nor press Select");
  layer_mark_dirty(s_canvas);
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int cx = bounds.size.w / 2;
  // Shift down 16px for status bar, then center
  int cy = 16 + (bounds.size.h - 16) / 2 - 10;

  // Draw the 8-ball circle
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, GPoint(cx, cy), 60);

  if (s_revealed) {
    // Compute eased scale
    int sc = ease_out(s_scale_pct);  // 0..100

    // Full-size triangle vertices relative to center (cx, cy)
    // Apex: (cx, cy-35), BL: (cx-30, cy+20), BR: (cx+30, cy+20)
    // Centroid Y = (cy-35 + cy+20 + cy+20)/3 = cy + 5/3 ~ cy+2
    int centroid_y = cy + 2;

    // Offsets from centroid at full scale
    // apex: (0, -37), bl: (-30, +18), br: (+30, +18)
    int apex_dy = -37 * sc / 100;
    int bot_dy  =  18 * sc / 100;
    int half_w  =  30 * sc / 100;

    GPoint tri[3] = {
      GPoint(cx, centroid_y + apex_dy),
      GPoint(cx - half_w, centroid_y + bot_dy),
      GPoint(cx + half_w, centroid_y + bot_dy),
    };

    // Color-coded triangle by category
    GColor tri_color = category_color(get_category(s_current));

    graphics_context_set_fill_color(ctx, tri_color);

    GPathInfo path_info = {
      .num_points = 3,
      .points = tri,
    };
    GPath *path = gpath_create(&path_info);
    gpath_draw_filled(ctx, path);
    gpath_destroy(path);
  } else {
    // Draw the "8"
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "8",
                       fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
                       GRect(cx - 20, cy - 28, 40, 50),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }

  // Draw history dots at bottom
  if (s_history_count > 0) {
    int dot_y = bounds.size.h - 10;
    int total_w = s_history_count * 8 + (s_history_count - 1) * 4;
    int start_x = (bounds.size.w - total_w) / 2;

    for (int i = 0; i < s_history_count; i++) {
      int idx = s_history[i];
      if (idx < 0) continue;

      GColor dot_color = s_theme.muted;

      graphics_context_set_fill_color(ctx, dot_color);
      int dx = start_x + i * 12 + 4;
      graphics_fill_circle(ctx, GPoint(dx, dot_y), 3);
    }
  }
}

// --- Overlay for first-launch ---
static void overlay_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
}

static void dismiss_overlay(void) {
  s_first_launch = false;
  if (s_overlay_layer) {
    layer_set_hidden(s_overlay_layer, true);
  }
  if (s_overlay_text) {
    layer_set_hidden(text_layer_get_layer(s_overlay_text), true);
  }
}

// AppMessage: receive config from phone
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *vib_t = dict_find(iter, MSG_KEY_VIBRATION);
  if (vib_t) {
    s_vibration_enabled = (vib_t->value->int32 != 0);
    persist_write_bool(STORAGE_KEY_VIBRATION, s_vibration_enabled);
  }

  Tuple *theme_t = dict_find(iter, PASTEL_MSG_KEY_THEME);
  if (theme_t) {
    s_theme_id = theme_t->value->int32;
    persist_write_int(PASTEL_STORAGE_KEY_THEME, s_theme_id);
    s_theme = pastel_get_theme(s_theme_id);
    // Update UI colors
    status_bar_layer_set_colors(s_status_bar, GColorBlack, s_theme.primary);
    text_layer_set_text_color(s_answer_layer, s_theme.primary);
    text_layer_set_text_color(s_prompt_layer, s_theme.accent);
    layer_mark_dirty(s_canvas);
  }

  Tuple *speed_t = dict_find(iter, MSG_KEY_ANIM_SPEED);
  if (speed_t) {
    s_anim_frame_ms = (int)speed_t->value->int32;
    // Clamp to sane range
    if (s_anim_frame_ms < 10) s_anim_frame_ms = 10;
    if (s_anim_frame_ms > 120) s_anim_frame_ms = 120;
    persist_write_int(STORAGE_KEY_ANIM_SPEED, s_anim_frame_ms);
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", reason);
}

// --- BT connection handler ---
static void bt_handler(bool connected) {
  if (!connected) {
    vibes_double_pulse();
  }
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  reveal_answer();
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  // Dismiss overlay on any button if first launch
  if (s_first_launch) {
    dismiss_overlay();
    return;
  }
  if (s_revealed) {
    reset_ball();
  } else {
    reveal_answer();
  }
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  if (s_first_launch) {
    dismiss_overlay();
    return;
  }
  reveal_answer();
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  if (s_first_launch) {
    dismiss_overlay();
    return;
  }
  reset_ball();
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

  // Status bar at top
  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, GColorBlack, s_theme.primary);
  status_bar_layer_set_separator_mode(s_status_bar, StatusBarLayerSeparatorModeDotted);
  layer_add_child(root, status_bar_layer_get_layer(s_status_bar));

  // Canvas for the 8-ball drawing
  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  int cx = bounds.size.w / 2;
  // Shift center down 16px for status bar
  int cy = 16 + (bounds.size.h - 16) / 2 - 10;

  // Answer text (inside the triangle area)
  s_answer_layer = text_layer_create(GRect(cx - 40, cy - 25, 80, 50));
  text_layer_set_background_color(s_answer_layer, GColorClear);
  text_layer_set_text_color(s_answer_layer, s_theme.primary);
  text_layer_set_font(s_answer_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_answer_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_answer_layer));

  // Prompt text at bottom (above history dots)
  s_prompt_layer = text_layer_create(GRect(0, bounds.size.h - 40, bounds.size.w, 30));
  text_layer_set_background_color(s_prompt_layer, GColorClear);
  text_layer_set_text_color(s_prompt_layer, s_theme.accent);
  text_layer_set_font(s_prompt_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_prompt_layer, GTextAlignmentCenter);
  text_layer_set_text(s_prompt_layer, "Shake me\nor press Select");
  layer_add_child(root, text_layer_get_layer(s_prompt_layer));

  // First-launch overlay
  s_overlay_layer = layer_create(bounds);
  layer_set_update_proc(s_overlay_layer, overlay_update);
  layer_add_child(root, s_overlay_layer);

  s_overlay_text = text_layer_create(GRect(10, bounds.size.h / 2 - 40, bounds.size.w - 20, 80));
  text_layer_set_background_color(s_overlay_text, GColorClear);
  text_layer_set_text_color(s_overlay_text, s_theme.highlight);
  text_layer_set_font(s_overlay_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_overlay_text, GTextAlignmentCenter);
  text_layer_set_text(s_overlay_text, "Shake or press\nany button!");
  layer_add_child(root, text_layer_get_layer(s_overlay_text));
}

static void window_unload(Window *window) {
  text_layer_destroy(s_overlay_text);
  layer_destroy(s_overlay_layer);
  text_layer_destroy(s_answer_layer);
  text_layer_destroy(s_prompt_layer);
  layer_destroy(s_canvas);
  status_bar_layer_destroy(s_status_bar);
}

static void init(void) {
  srand(time(NULL));

  // Load theme
  if (persist_exists(PASTEL_STORAGE_KEY_THEME)) {
    s_theme_id = persist_read_int(PASTEL_STORAGE_KEY_THEME);
  }
  s_theme = pastel_get_theme(s_theme_id);

  // Load persisted config
  if (persist_exists(STORAGE_KEY_VIBRATION)) {
    s_vibration_enabled = persist_read_bool(STORAGE_KEY_VIBRATION);
  }
  if (persist_exists(STORAGE_KEY_ANIM_SPEED)) {
    s_anim_frame_ms = persist_read_int(STORAGE_KEY_ANIM_SPEED);
  }

  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  accel_tap_service_subscribe(tap_handler);
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bt_handler,
  });

  // Set up AppMessage for config
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_open(64, 64);
}

static void deinit(void) {
  connection_service_unsubscribe();
  accel_tap_service_unsubscribe();
  if (s_anim_timer) {
    app_timer_cancel(s_anim_timer);
    s_anim_timer = NULL;
  }
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
