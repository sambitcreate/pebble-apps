#include <pebble.h>

// ── Morse code for digits 0-9 ──
// dot=0, dash=1, terminated by -1
static const int8_t MORSE_DIGITS[10][6] = {
  {1,1,1,1,1,-1},  // 0: -----
  {0,1,1,1,1,-1},  // 1: .----
  {0,0,1,1,1,-1},  // 2: ..---
  {0,0,0,1,1,-1},  // 3: ...--
  {0,0,0,0,1,-1},  // 4: ....-
  {0,0,0,0,0,-1},  // 5: .....
  {1,0,0,0,0,-1},  // 6: -....
  {1,1,0,0,0,-1},  // 7: --...
  {1,1,1,0,0,-1},  // 8: ---..
  {1,1,1,1,0,-1},  // 9: ----.
};

// ── Bar drawing constants ──
#define DOT_BAR_W   8
#define DASH_BAR_W  24
#define BAR_HEIGHT  6
#define BAR_GAP     4
#define DIGIT_GAP   12   // gap between digit groups
#define COLON_GAP   16   // gap for HH:MM separator

// ── Speed presets ──
typedef struct {
  const char *label;
  uint32_t dot_ms;
  uint32_t dash_ms;
} SpeedPreset;

static const SpeedPreset SPEED_PRESETS[] = {
  {"Slow",   150, 450},
  {"Normal", 100, 300},
  {"Fast",    60, 180},
};
#define NUM_SPEEDS 3

// ── Timing (derived from speed) ──
#define GAP_MS    100
#define CHAR_GAP  300
#define WORD_GAP  700
#define MAX_VIBE  64

// ── Persist keys ──
#define KEY_24H          0
#define KEY_SPEED        1
#define KEY_FIRST_LAUNCH 2

// ── UI layers ──
static Window *s_window;
static StatusBarLayer *s_status_bar;
static TextLayer *s_time_layer;
static Layer *s_bars_layer;          // canvas for morse bars
static TextLayer *s_speed_layer;     // speed indicator
static TextLayer *s_hint_layer;

// First-launch overlay
static Layer *s_overlay_layer;
static TextLayer *s_overlay_text;
static bool s_show_overlay = false;

// ── State ──
static uint32_t s_vibe_segments[MAX_VIBE];
static int s_vibe_count;
static bool s_24h = true;
static int s_speed_index = 1;        // default: Normal
static char s_time_buf[8];
static char s_speed_buf[16];

// ── Active element tracking for animation ──
// Flat index into all elements across 4 digits: digit0_elem0, digit0_elem1, ...
static int s_active_element = -1;    // which element is currently vibrating (-1 = none)
static AppTimer *s_anim_timer = NULL;

// The 4 digits of HH:MM for bar drawing
static int s_digits[4] = {0, 0, 0, 0};

// Playback state
static int s_play_digit = 0;         // which digit (0-3) we're playing
static int s_play_elem = 0;          // which element within that digit
static bool s_playing = false;
static bool s_play_minutes_only = false;

// Flat element index (for highlight)
static int s_flat_index = 0;

// ── Status bar offset ──
#define STATUS_BAR_HEIGHT 16

// ── Forward declarations ──
static void update_display(void);
static void advance_playback(void *data);
static void clear_highlight(void *data);

// ── Compute flat element index for a given digit/elem ──
static int compute_flat_index(int digit_idx, int elem_idx) {
  int flat = 0;
  for (int d = 0; d < digit_idx; d++) {
    for (int e = 0; MORSE_DIGITS[s_digits[d]][e] != -1; e++) {
      flat++;
    }
  }
  flat += elem_idx;
  return flat;
}

// ── Draw morse bars ──
static void bars_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // We draw 4 digit groups: [d0] [d1] : [d2] [d3]
  // First pass: compute total width
  int total_w = 0;
  for (int d = 0; d < 4; d++) {
    int dw = 0;
    for (int e = 0; MORSE_DIGITS[s_digits[d]][e] != -1; e++) {
      if (e > 0) dw += BAR_GAP;
      dw += (MORSE_DIGITS[s_digits[d]][e] == 0) ? DOT_BAR_W : DASH_BAR_W;
    }
    total_w += dw;
    if (d < 3) {
      total_w += (d == 1) ? COLON_GAP : DIGIT_GAP;
    }
  }

  int x = (bounds.size.w - total_w) / 2;
  int y = (bounds.size.h - BAR_HEIGHT) / 2;
  int flat = 0;

  for (int d = 0; d < 4; d++) {
    for (int e = 0; MORSE_DIGITS[s_digits[d]][e] != -1; e++) {
      if (e > 0) x += BAR_GAP;

      int bar_w = (MORSE_DIGITS[s_digits[d]][e] == 0) ? DOT_BAR_W : DASH_BAR_W;

      // Choose color: active element gets bright highlight
      if (flat == s_active_element) {
        graphics_context_set_fill_color(ctx,
          PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
      } else {
        graphics_context_set_fill_color(ctx,
          PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
      }

      graphics_fill_rect(ctx, GRect(x, y, bar_w, BAR_HEIGHT), 1, GCornersAll);
      x += bar_w;
      flat++;
    }

    // Gap after digit group
    if (d < 3) {
      if (d == 1) {
        // Draw colon separator between hours and minutes
        int cx = x + COLON_GAP / 2;
        int dot_r = 2;
        graphics_context_set_fill_color(ctx,
          PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
        graphics_fill_circle(ctx, GPoint(cx, y - 2), dot_r);
        graphics_fill_circle(ctx, GPoint(cx, y + BAR_HEIGHT + 2), dot_r);
        x += COLON_GAP;
      } else {
        x += DIGIT_GAP;
      }
    }
  }
}

// ── Build vibe for a single digit using current speed ──
static void build_vibe_for_digit(int digit) {
  uint32_t dot_ms = SPEED_PRESETS[s_speed_index].dot_ms;
  uint32_t dash_ms = SPEED_PRESETS[s_speed_index].dash_ms;

  for (int i = 0; MORSE_DIGITS[digit][i] != -1 && s_vibe_count < MAX_VIBE - 1; i++) {
    if (i > 0 && s_vibe_count < MAX_VIBE) {
      s_vibe_segments[s_vibe_count++] = GAP_MS;
    }
    s_vibe_segments[s_vibe_count++] = MORSE_DIGITS[digit][i] ? dash_ms : dot_ms;
  }
}

// ── Vibrate the full time (or just minutes) ──
static void vibrate_time(bool minutes_only) {
  s_vibe_count = 0;

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int h = s_24h ? t->tm_hour : (t->tm_hour % 12 == 0 ? 12 : t->tm_hour % 12);
  int m = t->tm_min;

  const int digits[4] = {h / 10, h % 10, m / 10, m % 10};
  int start = minutes_only ? 2 : 0;

  for (int d = start; d < 4; d++) {
    if (d > start && s_vibe_count < MAX_VIBE) {
      s_vibe_segments[s_vibe_count++] = CHAR_GAP;
    }
    if (d == 2 && !minutes_only && s_vibe_count < MAX_VIBE) {
      s_vibe_segments[s_vibe_count++] = WORD_GAP;
    }
    build_vibe_for_digit(digits[d]);
  }

  if (s_vibe_count > 0) {
    VibePattern pat = {
      .durations = s_vibe_segments,
      .num_segments = s_vibe_count,
    };
    vibes_enqueue_custom_pattern(pat);
  }
}

// ── Playback animation: highlight bars one by one ──

static void start_playback(bool minutes_only) {
  // Cancel any existing playback
  if (s_anim_timer) {
    app_timer_cancel(s_anim_timer);
    s_anim_timer = NULL;
  }

  s_playing = true;
  s_play_minutes_only = minutes_only;
  s_play_digit = minutes_only ? 2 : 0;
  s_play_elem = 0;
  s_flat_index = compute_flat_index(s_play_digit, 0);

  // Fire vibration
  vibrate_time(minutes_only);

  // Start highlight animation
  s_active_element = s_flat_index;
  layer_mark_dirty(s_bars_layer);

  // Schedule next step
  uint32_t dot_ms = SPEED_PRESETS[s_speed_index].dot_ms;
  uint32_t dash_ms = SPEED_PRESETS[s_speed_index].dash_ms;
  int8_t elem = MORSE_DIGITS[s_digits[s_play_digit]][s_play_elem];
  uint32_t dur = (elem == 0) ? dot_ms : dash_ms;
  s_anim_timer = app_timer_register(dur, clear_highlight, NULL);
}

static void clear_highlight(void *data) {
  // Turn off current highlight
  s_active_element = -1;
  layer_mark_dirty(s_bars_layer);

  // Determine delay before next element
  uint32_t delay;

  // Check if there's a next element in this digit
  int next_elem = s_play_elem + 1;
  if (MORSE_DIGITS[s_digits[s_play_digit]][next_elem] != -1) {
    // Next element in same digit — inter-element gap
    s_play_elem = next_elem;
    delay = GAP_MS;
  } else {
    // Move to next digit
    int next_digit = s_play_digit + 1;
    if (next_digit >= 4) {
      // Done
      s_playing = false;
      s_anim_timer = NULL;
      return;
    }
    s_play_digit = next_digit;
    s_play_elem = 0;

    // If crossing from hours to minutes (digit 1 -> 2), add word gap
    if (next_digit == 2 && !s_play_minutes_only) {
      delay = CHAR_GAP + WORD_GAP;
    } else {
      delay = CHAR_GAP;
    }
  }

  s_flat_index = compute_flat_index(s_play_digit, s_play_elem);
  s_anim_timer = app_timer_register(delay, advance_playback, NULL);
}

static void advance_playback(void *data) {
  // Highlight current element
  s_active_element = s_flat_index;
  layer_mark_dirty(s_bars_layer);

  // Schedule clear after element duration
  uint32_t dot_ms = SPEED_PRESETS[s_speed_index].dot_ms;
  uint32_t dash_ms = SPEED_PRESETS[s_speed_index].dash_ms;
  int8_t elem = MORSE_DIGITS[s_digits[s_play_digit]][s_play_elem];
  uint32_t dur = (elem == 0) ? dot_ms : dash_ms;
  s_anim_timer = app_timer_register(dur, clear_highlight, NULL);
}

// ── Update display ──
static void update_display(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  int h = s_24h ? t->tm_hour : (t->tm_hour % 12 == 0 ? 12 : t->tm_hour % 12);
  int m = t->tm_min;

  snprintf(s_time_buf, sizeof(s_time_buf), "%02d:%02d", h, m);
  text_layer_set_text(s_time_layer, s_time_buf);

  // Store digits for bar drawing
  s_digits[0] = h / 10;
  s_digits[1] = h % 10;
  s_digits[2] = m / 10;
  s_digits[3] = m % 10;

  // Update speed indicator
  snprintf(s_speed_buf, sizeof(s_speed_buf), "%s", SPEED_PRESETS[s_speed_index].label);
  text_layer_set_text(s_speed_layer, s_speed_buf);

  // Redraw bars
  layer_mark_dirty(s_bars_layer);
}

// ── Overlay ──
static void dismiss_overlay(void) {
  if (s_show_overlay && s_overlay_layer) {
    s_show_overlay = false;
    layer_set_hidden(s_overlay_layer, true);
  }
}

// ── Button handlers ──
static void select_click(ClickRecognizerRef recognizer, void *context) {
  if (s_show_overlay) { dismiss_overlay(); return; }
  start_playback(false);
  text_layer_set_text(s_hint_layer, "Vibrating time...");
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  if (s_show_overlay) { dismiss_overlay(); return; }
  s_24h = !s_24h;
  persist_write_bool(KEY_24H, s_24h);
  update_display();
  text_layer_set_text(s_hint_layer, s_24h ? "24h mode" : "12h mode");
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  if (s_show_overlay) { dismiss_overlay(); return; }
  start_playback(true);
  text_layer_set_text(s_hint_layer, "Vibrating minutes...");
}

static void up_long_click(ClickRecognizerRef recognizer, void *context) {
  if (s_show_overlay) { dismiss_overlay(); return; }
  s_speed_index = (s_speed_index + 1) % NUM_SPEEDS;
  persist_write_int(KEY_SPEED, s_speed_index);
  update_display();

  // Show speed change in hint
  snprintf(s_speed_buf, sizeof(s_speed_buf), "Speed: %s",
           SPEED_PRESETS[s_speed_index].label);
  text_layer_set_text(s_hint_layer, s_speed_buf);

  // Confirm vibration
  static const uint32_t segs[] = {40};
  VibePattern pat = { .durations = segs, .num_segments = 1 };
  vibes_enqueue_custom_pattern(pat);
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_long_click_subscribe(BUTTON_ID_UP, 700, up_long_click, NULL);
}

// ── Tick handler ──
static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  update_display();
}

// ── BT handler ──
static void bt_handler(bool connected) {
  if (!connected) {
    static const uint32_t segs[] = {200, 100, 200, 100, 200};
    VibePattern pat = { .durations = segs, .num_segments = 5 };
    vibes_enqueue_custom_pattern(pat);
  }
}

// ── Overlay draw ──
static void overlay_update_proc(Layer *layer, GContext *ctx) {
  if (!s_show_overlay) return;
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
}

// ── Window load ──
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

  int y_offset = STATUS_BAR_HEIGHT;
  int content_w = bounds.size.w;
  int content_h = bounds.size.h - y_offset;

  // Digital time (large)
  s_time_layer = text_layer_create(GRect(0, y_offset + 10, content_w, 50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer,
    fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  // 2. Morse bars canvas layer
  s_bars_layer = layer_create(GRect(0, y_offset + 68, content_w, 30));
  layer_set_update_proc(s_bars_layer, bars_update_proc);
  layer_add_child(root, s_bars_layer);

  // Speed indicator
  s_speed_layer = text_layer_create(GRect(0, y_offset + 96, content_w, 20));
  text_layer_set_background_color(s_speed_layer, GColorClear);
  text_layer_set_text_color(s_speed_layer,
    PBL_IF_COLOR_ELSE(GColorCyan, GColorWhite));
  text_layer_set_font(s_speed_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_speed_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_speed_layer));

  // Hint line
  s_hint_layer = text_layer_create(
    GRect(0, y_offset + content_h - 24, content_w, 24));
  text_layer_set_background_color(s_hint_layer, GColorClear);
  text_layer_set_text_color(s_hint_layer,
    PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
  text_layer_set_font(s_hint_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_hint_layer, GTextAlignmentCenter);
  text_layer_set_text(s_hint_layer, "Sel: play  Hold Up: speed");
  layer_add_child(root, text_layer_get_layer(s_hint_layer));

  // 4. First-launch overlay
  if (s_show_overlay) {
    s_overlay_layer = layer_create(bounds);
    layer_set_update_proc(s_overlay_layer, overlay_update_proc);
    layer_add_child(root, s_overlay_layer);

    s_overlay_text = text_layer_create(
      GRect(10, bounds.size.h / 2 - 55, bounds.size.w - 20, 110));
    text_layer_set_background_color(s_overlay_text, GColorClear);
    text_layer_set_text_color(s_overlay_text,
      PBL_IF_COLOR_ELSE(GColorCyan, GColorWhite));
    text_layer_set_font(s_overlay_text,
      fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_text_alignment(s_overlay_text, GTextAlignmentCenter);
    text_layer_set_text(s_overlay_text,
      "Morse Time v1.1\n\n"
      "SEL: Vibrate time\n"
      "UP: 12h/24h toggle\n"
      "DOWN: Minutes only\n"
      "Hold UP: Speed\n\n"
      "Press any button");
    layer_add_child(s_overlay_layer, text_layer_get_layer(s_overlay_text));
  }

  update_display();
}

// ── Window unload ──
static void window_unload(Window *window) {
  status_bar_layer_destroy(s_status_bar);
  text_layer_destroy(s_time_layer);
  layer_destroy(s_bars_layer);
  text_layer_destroy(s_speed_layer);
  text_layer_destroy(s_hint_layer);
  if (s_overlay_layer) {
    if (s_overlay_text) text_layer_destroy(s_overlay_text);
    layer_destroy(s_overlay_layer);
  }
}

// ── Init ──
static void init(void) {
  // Load persisted settings
  if (persist_exists(KEY_24H)) {
    s_24h = persist_read_bool(KEY_24H);
  }
  if (persist_exists(KEY_SPEED)) {
    s_speed_index = persist_read_int(KEY_SPEED);
    if (s_speed_index < 0 || s_speed_index >= NUM_SPEEDS) {
      s_speed_index = 1;
    }
  }

  // First-launch check
  if (!persist_exists(KEY_FIRST_LAUNCH)) {
    s_show_overlay = true;
    persist_write_bool(KEY_FIRST_LAUNCH, true);
  }

  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // BT disconnect alert
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bt_handler,
  });
}

// ── Deinit ──
static void deinit(void) {
  tick_timer_service_unsubscribe();
  connection_service_unsubscribe();
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
