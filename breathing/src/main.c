#include <pebble.h>
#include "../../shared/pebble_pastel.h"

// ---------------------------------------------------------------------------
// Persistent storage keys
// ---------------------------------------------------------------------------
#define PERSIST_KEY_PATTERN    1
#define PERSIST_KEY_FIRST_RUN  2
#define PERSIST_KEY_DURATION   3

// ---------------------------------------------------------------------------
// AppMessage keys (must match messageKeys in package.json)
// ---------------------------------------------------------------------------
#define MSG_KEY_PATTERN  0
#define MSG_KEY_DURATION 1

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define MIN_RADIUS 10
#define MAX_RADIUS 55
#define TICK_MS    100
#define NUM_RINGS  3
#define SB_HEIGHT  STATUS_BAR_LAYER_HEIGHT  // 16px

// ---------------------------------------------------------------------------
// Data types
// ---------------------------------------------------------------------------
typedef enum {
  BREATH_INHALE,
  BREATH_HOLD,
  BREATH_EXHALE,
  BREATH_HOLD2,
} BreathPhase;

typedef struct {
  const char *name;
  int inhale;   // seconds
  int hold1;
  int exhale;
  int hold2;    // 0 if no second hold
} Pattern;

// ---------------------------------------------------------------------------
// Patterns
// ---------------------------------------------------------------------------
static const Pattern PATTERNS[] = {
  {"4-7-8",   4, 7, 8, 0},
  {"Box",     4, 4, 4, 4},
  {"Simple",  4, 0, 4, 0},
};
#define NUM_PATTERNS 3

// Session duration presets (seconds)
static const int SESSION_DURATIONS[] = {60, 120, 300, 600};
static const char *SESSION_LABELS[]  = {"1 min", "2 min", "5 min", "10 min"};
#define NUM_DURATIONS 4

// ---------------------------------------------------------------------------
// UI layers
// ---------------------------------------------------------------------------
static PastelTheme s_theme;
static int s_theme_id = THEME_WARM_SUNSET;

static Window *s_window;
static StatusBarLayer *s_status_bar;
static Layer *s_canvas;
static TextLayer *s_phase_layer;
static TextLayer *s_pattern_layer;
static TextLayer *s_count_layer;
static TextLayer *s_duration_layer;

// Instruction overlay layers
static Layer *s_overlay_layer;
static TextLayer *s_instr_title;
static TextLayer *s_instr_body;
static bool s_show_overlay = false;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static int s_pattern_idx = 0;
static BreathPhase s_phase = BREATH_INHALE;
static int s_phase_elapsed = 0;   // tenths of seconds into current phase
static int s_phase_duration = 0;  // tenths of seconds
static int s_cycles = 0;
static bool s_running = false;
static char s_count_buf[16];
static char s_dur_buf[24];

// Session duration
static int s_duration_idx = 0;     // index into SESSION_DURATIONS
static int s_session_elapsed = 0;  // tenths of seconds elapsed in session

// Concentric rings
static int s_ring_radii[NUM_RINGS];
static const int s_ring_offsets[NUM_RINGS] = {0, 8, 16}; // stagger in ticks

// Haptic
static AppTimer *s_haptic_timer;
static int s_haptic_step = 0;

// Tick timer
static AppTimer *s_tick_timer;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void tick_callback(void *data);
static void stop_session(void);
static void haptic_cancel(void);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static int phase_seconds(BreathPhase phase) {
  const Pattern *p = &PATTERNS[s_pattern_idx];
  switch (phase) {
    case BREATH_INHALE: return p->inhale;
    case BREATH_HOLD:   return p->hold1;
    case BREATH_EXHALE: return p->exhale;
    case BREATH_HOLD2:  return p->hold2;
  }
  return 0;
}

static const char *phase_name(BreathPhase phase) {
  switch (phase) {
    case BREATH_INHALE: return "Breathe In";
    case BREATH_HOLD:
    case BREATH_HOLD2:  return "Hold";
    case BREATH_EXHALE: return "Breathe Out";
  }
  return "";
}

// ---------------------------------------------------------------------------
// Haptic patterns per phase
// ---------------------------------------------------------------------------
static void haptic_inhale_tick(void *data);
static void haptic_exhale_tick(void *data);

// Inhale: continuous gentle buzz (repeating short pulses every 400ms)
static void haptic_inhale_tick(void *data) {
  if (!s_running || s_phase != BREATH_INHALE) return;
  vibes_short_pulse();
  s_haptic_timer = app_timer_register(400, haptic_inhale_tick, NULL);
}

// Exhale: pulsing pattern (short pulse, pause, short pulse, pause...)
static void haptic_exhale_tick(void *data) {
  if (!s_running || s_phase != BREATH_EXHALE) return;
  s_haptic_step++;
  if (s_haptic_step % 2 == 1) {
    // Pulse
    vibes_short_pulse();
    s_haptic_timer = app_timer_register(200, haptic_exhale_tick, NULL);
  } else {
    // Silence gap
    s_haptic_timer = app_timer_register(400, haptic_exhale_tick, NULL);
  }
}

static void haptic_cancel(void) {
  if (s_haptic_timer) {
    app_timer_cancel(s_haptic_timer);
    s_haptic_timer = NULL;
  }
}

static void haptic_start_for_phase(BreathPhase phase) {
  haptic_cancel();
  s_haptic_step = 0;
  switch (phase) {
    case BREATH_INHALE:
      vibes_short_pulse();
      s_haptic_timer = app_timer_register(400, haptic_inhale_tick, NULL);
      break;
    case BREATH_EXHALE:
      s_haptic_step = 1; // start with a pulse
      vibes_short_pulse();
      s_haptic_timer = app_timer_register(200, haptic_exhale_tick, NULL);
      break;
    case BREATH_HOLD:
    case BREATH_HOLD2:
      // Silence — no vibration during hold
      break;
  }
}

// ---------------------------------------------------------------------------
// Phase management
// ---------------------------------------------------------------------------
static void start_phase(BreathPhase phase) {
  // Skip phases with 0 duration
  while (phase_seconds(phase) == 0) {
    phase = (phase + 1) % 4;
    if (phase == BREATH_INHALE) {
      s_cycles++;
    }
  }

  s_phase = phase;
  s_phase_elapsed = 0;
  s_phase_duration = phase_seconds(phase) * 10;  // convert to tenths

  text_layer_set_text(s_phase_layer, phase_name(phase));

  // Haptic guide for this phase
  haptic_start_for_phase(phase);
}

static void next_phase(void) {
  BreathPhase next = (s_phase + 1) % 4;
  if (next == BREATH_INHALE) {
    s_cycles++;
  }
  start_phase(next);
}

// ---------------------------------------------------------------------------
// Session end
// ---------------------------------------------------------------------------
static void stop_session(void) {
  s_running = false;
  haptic_cancel();
  if (s_tick_timer) {
    app_timer_cancel(s_tick_timer);
    s_tick_timer = NULL;
  }

  // Show summary
  snprintf(s_count_buf, sizeof(s_count_buf), "Breaths: %d", s_cycles);
  text_layer_set_text(s_count_layer, s_count_buf);
  text_layer_set_text(s_phase_layer, "Session Done");

  // Double vibe to signal end
  static const uint32_t segments[] = {200, 100, 200};
  VibePattern pat = { .durations = segments, .num_segments = 3 };
  vibes_enqueue_custom_pattern(pat);
}

// ---------------------------------------------------------------------------
// Canvas: concentric rings
// ---------------------------------------------------------------------------
static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int cx = bounds.size.w / 2;
  int cy = SB_HEIGHT + (bounds.size.h - SB_HEIGHT) / 2 - 10;

  // Outer boundary ring (guide circle)
  graphics_context_set_stroke_color(ctx, s_theme.muted);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, GPoint(cx, cy), MAX_RADIUS + 4);

  // Ring colors from theme: inner = highlight, middle = accent, outer = secondary
  GColor ring_colors[NUM_RINGS];
  ring_colors[0] = s_theme.highlight;
  ring_colors[1] = s_theme.accent;
  ring_colors[2] = s_theme.secondary;

  int stroke_widths[NUM_RINGS] = {4, 2, 1};

  // Draw from outermost to innermost so inner overlaps outer
  for (int i = NUM_RINGS - 1; i >= 0; i--) {
    if (s_ring_radii[i] > 0) {
      graphics_context_set_stroke_color(ctx, ring_colors[i]);
      graphics_context_set_stroke_width(ctx, stroke_widths[i]);
      graphics_draw_circle(ctx, GPoint(cx, cy), s_ring_radii[i]);
    }
  }

  // Small filled circle at center for visual anchor
  graphics_context_set_fill_color(ctx, s_theme.highlight);
  int fill_r = s_ring_radii[0] > 4 ? s_ring_radii[0] - 3 : s_ring_radii[0];
  if (fill_r < 2) fill_r = 2;
  graphics_fill_circle(ctx, GPoint(cx, cy), fill_r);

  // Cycle count
  snprintf(s_count_buf, sizeof(s_count_buf), "Breaths: %d", s_cycles);
  text_layer_set_text(s_count_layer, s_count_buf);
}

// ---------------------------------------------------------------------------
// Tick timer
// ---------------------------------------------------------------------------
static void tick_callback(void *data) {
  if (!s_running) return;

  s_phase_elapsed++;
  s_session_elapsed++;

  // Check session end
  if (s_session_elapsed >= SESSION_DURATIONS[s_duration_idx] * 10) {
    stop_session();
    return;
  }

  // Update concentric ring radii
  for (int i = 0; i < NUM_RINGS; i++) {
    int adjusted = s_phase_elapsed - s_ring_offsets[i];
    if (adjusted < 0) adjusted = 0;
    float prog = (float)adjusted / (float)s_phase_duration;
    if (prog > 1.0f) prog = 1.0f;

    switch (s_phase) {
      case BREATH_INHALE:
        s_ring_radii[i] = MIN_RADIUS + (int)((MAX_RADIUS - MIN_RADIUS) * prog);
        break;
      case BREATH_HOLD:
        s_ring_radii[i] = MAX_RADIUS;
        break;
      case BREATH_HOLD2:
        s_ring_radii[i] = MIN_RADIUS;
        break;
      case BREATH_EXHALE:
        s_ring_radii[i] = MAX_RADIUS - (int)((MAX_RADIUS - MIN_RADIUS) * prog);
        break;
    }
  }

  layer_mark_dirty(s_canvas);

  if (s_phase_elapsed >= s_phase_duration) {
    next_phase();
  }

  s_tick_timer = app_timer_register(TICK_MS, tick_callback, NULL);
}

// ---------------------------------------------------------------------------
// Overlay (first-launch instructions)
// ---------------------------------------------------------------------------
static void overlay_update(Layer *layer, GContext *ctx) {
  if (!s_show_overlay) return;
  GRect bounds = layer_get_bounds(layer);

  // Semi-transparent background (solid black on B&W)
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
}

static void dismiss_overlay(void) {
  if (!s_show_overlay) return;
  s_show_overlay = false;
  layer_set_hidden(s_overlay_layer, true);
  layer_set_hidden(text_layer_get_layer(s_instr_title), true);
  layer_set_hidden(text_layer_get_layer(s_instr_body), true);
  // Mark that we've shown the overlay
  persist_write_bool(PERSIST_KEY_FIRST_RUN, false);
}

// ---------------------------------------------------------------------------
// AppMessage: receive config from phone
// ---------------------------------------------------------------------------
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *pattern_t = dict_find(iter, MSG_KEY_PATTERN);
  if (pattern_t) {
    int val = pattern_t->value->int32;
    if (val >= 0 && val < NUM_PATTERNS) {
      s_pattern_idx = val;
      persist_write_int(PERSIST_KEY_PATTERN, s_pattern_idx);
      text_layer_set_text(s_pattern_layer, PATTERNS[s_pattern_idx].name);
      if (s_running) {
        start_phase(BREATH_INHALE);
      }
    }
  }
  Tuple *duration_t = dict_find(iter, MSG_KEY_DURATION);
  if (duration_t) {
    int val = duration_t->value->int32;
    if (val >= 0 && val < NUM_DURATIONS) {
      s_duration_idx = val;
      persist_write_int(PERSIST_KEY_DURATION, s_duration_idx);
      if (!s_running) {
        snprintf(s_dur_buf, sizeof(s_dur_buf), "Session: %s", SESSION_LABELS[s_duration_idx]);
        text_layer_set_text(s_duration_layer, s_dur_buf);
      }
    }
  }

  Tuple *theme_t = dict_find(iter, PASTEL_MSG_KEY_THEME);
  if (theme_t) {
    s_theme_id = theme_t->value->int32;
    persist_write_int(PASTEL_STORAGE_KEY_THEME, s_theme_id);
    s_theme = pastel_get_theme(s_theme_id);
    // Re-apply theme colors
    text_layer_set_text_color(s_phase_layer, s_theme.primary);
    text_layer_set_text_color(s_pattern_layer, s_theme.secondary);
    text_layer_set_text_color(s_duration_layer, s_theme.muted);
    text_layer_set_text_color(s_count_layer, s_theme.muted);
    layer_mark_dirty(s_canvas);
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", reason);
}

// ---------------------------------------------------------------------------
// Click handlers
// ---------------------------------------------------------------------------
static void select_click(ClickRecognizerRef recognizer, void *context) {
  if (s_show_overlay) { dismiss_overlay(); return; }

  s_running = !s_running;
  if (s_running) {
    for (int i = 0; i < NUM_RINGS; i++) s_ring_radii[i] = MIN_RADIUS;
    s_cycles = 0;
    s_session_elapsed = 0;
    start_phase(BREATH_INHALE);
    // Hide duration label while running
    text_layer_set_text(s_duration_layer, "");
    s_tick_timer = app_timer_register(TICK_MS, tick_callback, NULL);
  } else {
    stop_session();
    text_layer_set_text(s_phase_layer, "Paused");
    // Show duration again
    snprintf(s_dur_buf, sizeof(s_dur_buf), "Session: %s", SESSION_LABELS[s_duration_idx]);
    text_layer_set_text(s_duration_layer, s_dur_buf);
  }
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
}

// ---------------------------------------------------------------------------
// Bluetooth disconnect alert
// ---------------------------------------------------------------------------
static void bt_handler(bool connected) {
  if (!connected) {
    static const uint32_t segments[] = {200, 100, 200, 100, 200};
    VibePattern pat = { .durations = segments, .num_segments = 5 };
    vibes_enqueue_custom_pattern(pat);
  }
}

// ---------------------------------------------------------------------------
// Window load / unload
// ---------------------------------------------------------------------------
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, GColorBlack);

  // Status bar at top with dotted separator
  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, GColorBlack,
                              PBL_IF_COLOR_ELSE(GColorWhite, GColorWhite));
  status_bar_layer_set_separator_mode(s_status_bar,
                                       StatusBarLayerSeparatorModeDotted);
  layer_add_child(root, status_bar_layer_get_layer(s_status_bar));

  // Pattern name — shifted down by SB_HEIGHT
  s_pattern_layer = text_layer_create(GRect(0, SB_HEIGHT + 2, bounds.size.w, 20));
  text_layer_set_background_color(s_pattern_layer, GColorClear);
  text_layer_set_text_color(s_pattern_layer, s_theme.secondary);
  text_layer_set_font(s_pattern_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_pattern_layer, GTextAlignmentCenter);
  text_layer_set_text(s_pattern_layer, PATTERNS[s_pattern_idx].name);
  layer_add_child(root, text_layer_get_layer(s_pattern_layer));

  // Session duration label
  s_duration_layer = text_layer_create(GRect(0, SB_HEIGHT + 16, bounds.size.w, 20));
  text_layer_set_background_color(s_duration_layer, GColorClear);
  text_layer_set_text_color(s_duration_layer, s_theme.muted);
  text_layer_set_font(s_duration_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_duration_layer, GTextAlignmentCenter);
  snprintf(s_dur_buf, sizeof(s_dur_buf), "Session: %s", SESSION_LABELS[s_duration_idx]);
  text_layer_set_text(s_duration_layer, s_dur_buf);
  layer_add_child(root, text_layer_get_layer(s_duration_layer));

  // Canvas for rings
  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  // Phase text — shifted up slightly to account for status bar
  s_phase_layer = text_layer_create(GRect(0, bounds.size.h - 48, bounds.size.w, 24));
  text_layer_set_background_color(s_phase_layer, GColorClear);
  text_layer_set_text_color(s_phase_layer, s_theme.primary);
  text_layer_set_font(s_phase_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_phase_layer, GTextAlignmentCenter);
  text_layer_set_text(s_phase_layer, "Press Select");
  layer_add_child(root, text_layer_get_layer(s_phase_layer));

  // Cycle count
  s_count_layer = text_layer_create(GRect(0, bounds.size.h - 24, bounds.size.w, 24));
  text_layer_set_background_color(s_count_layer, GColorClear);
  text_layer_set_text_color(s_count_layer, s_theme.muted);
  text_layer_set_font(s_count_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_count_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_count_layer));

  // --- First-launch instruction overlay ---
  if (s_show_overlay) {
    // Full-screen overlay background
    s_overlay_layer = layer_create(bounds);
    layer_set_update_proc(s_overlay_layer, overlay_update);
    layer_add_child(root, s_overlay_layer);

    // Title
    s_instr_title = text_layer_create(GRect(10, SB_HEIGHT + 20, bounds.size.w - 20, 30));
    text_layer_set_background_color(s_instr_title, GColorClear);
    text_layer_set_text_color(s_instr_title, GColorWhite);
    text_layer_set_font(s_instr_title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(s_instr_title, GTextAlignmentCenter);
    text_layer_set_text(s_instr_title, "Breathe");
    layer_add_child(root, text_layer_get_layer(s_instr_title));

    // Body
    s_instr_body = text_layer_create(GRect(8, SB_HEIGHT + 52, bounds.size.w - 16, 100));
    text_layer_set_background_color(s_instr_body, GColorClear);
    text_layer_set_text_color(s_instr_body, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
    text_layer_set_font(s_instr_body, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_text_alignment(s_instr_body, GTextAlignmentCenter);
    text_layer_set_text(s_instr_body,
      "SEL: Start/Stop\n"
      "Settings: Phone App\n"
      "Press any button...");
    layer_add_child(root, text_layer_get_layer(s_instr_body));
  }

  // Initialize ring radii
  for (int i = 0; i < NUM_RINGS; i++) s_ring_radii[i] = MIN_RADIUS;
}

static void window_unload(Window *window) {
  text_layer_destroy(s_phase_layer);
  text_layer_destroy(s_pattern_layer);
  text_layer_destroy(s_count_layer);
  text_layer_destroy(s_duration_layer);
  status_bar_layer_destroy(s_status_bar);
  layer_destroy(s_canvas);
  if (s_overlay_layer) {
    layer_destroy(s_overlay_layer);
    text_layer_destroy(s_instr_title);
    text_layer_destroy(s_instr_body);
  }
}

// ---------------------------------------------------------------------------
// Init / Deinit
// ---------------------------------------------------------------------------
static void init(void) {
  // Restore persisted pattern index
  if (persist_exists(PERSIST_KEY_PATTERN)) {
    s_pattern_idx = persist_read_int(PERSIST_KEY_PATTERN);
    if (s_pattern_idx < 0 || s_pattern_idx >= NUM_PATTERNS) {
      s_pattern_idx = 0;
    }
  }

  // Restore persisted duration index
  if (persist_exists(PERSIST_KEY_DURATION)) {
    s_duration_idx = persist_read_int(PERSIST_KEY_DURATION);
    if (s_duration_idx < 0 || s_duration_idx >= NUM_DURATIONS) {
      s_duration_idx = 0;
    }
  }

  // Load theme
  if (persist_exists(PASTEL_STORAGE_KEY_THEME)) {
    s_theme_id = persist_read_int(PASTEL_STORAGE_KEY_THEME);
  }
  s_theme = pastel_get_theme(s_theme_id);

  // Check first-launch
  if (!persist_exists(PERSIST_KEY_FIRST_RUN) || persist_read_bool(PERSIST_KEY_FIRST_RUN)) {
    s_show_overlay = true;
  }

  // Subscribe to Bluetooth connection changes
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bt_handler,
  });

  // Set up AppMessage for config from phone
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_open(64, 64);

  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

static void deinit(void) {
  haptic_cancel();
  if (s_tick_timer) app_timer_cancel(s_tick_timer);

  // Persist preferred pattern and duration
  persist_write_int(PERSIST_KEY_PATTERN, s_pattern_idx);
  persist_write_int(PERSIST_KEY_DURATION, s_duration_idx);

  connection_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
