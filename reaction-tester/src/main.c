#include <pebble.h>

// Persist keys
#define PERSIST_KEY_BEST 1
#define PERSIST_KEY_LAUNCHED 2
#define PERSIST_KEY_ROUNDS 3

// AppMessage key — must match messageKeys in package.json
#define MSG_KEY_ROUNDS 0

// Session config
#define MAX_ROUNDS_LIMIT 20   // static array ceiling
#define RING_INTERVAL_MS 20
#define RING_STEP_PX 6

static int s_max_rounds = 10; // configurable via phone settings

static Window *s_window;
static StatusBarLayer *s_status_bar;
static TextLayer *s_main_layer;
static TextLayer *s_result_layer;
static TextLayer *s_best_layer;
static TextLayer *s_hint_layer;
static TextLayer *s_round_layer;
static Layer *s_ring_layer;
static AppTimer *s_delay_timer;
static AppTimer *s_ring_timer;

typedef enum {
  STATE_IDLE,
  STATE_WAITING,
  STATE_READY,
  STATE_RESULT,
  STATE_TOO_EARLY,
  STATE_STATS,
  STATE_SUMMARY,
  STATE_OVERLAY,
} GameState;

static GameState s_state = STATE_IDLE;
static uint16_t s_trigger_time;
static uint16_t s_reaction_ms;
static uint16_t s_best_ms = 9999;
static uint16_t s_all_time_best_ms = 9999;
static char s_result_buf[32];
static char s_best_buf[48];
static char s_stats_buf[128];
static char s_round_buf[32];

// Session tracking
static uint16_t s_session_times[MAX_ROUNDS_LIMIT];
static int s_session_count = 0;
static int s_current_round = 0;

// Expanding ring
static int s_ring_radius = 0;
static bool s_ring_active = false;

// BT state
static bool s_bt_connected = true;

// First-launch flag
static bool s_first_launch = false;

// Content offset below status bar
#define STATUS_BAR_HEIGHT 16

// ---------- helpers ----------

static void set_bg(GColor color) {
  window_set_background_color(s_window, color);
}

static void update_round_display(void) {
  if (s_state == STATE_WAITING || s_state == STATE_READY) {
    snprintf(s_round_buf, sizeof(s_round_buf), "Round %d/%d", s_current_round, s_max_rounds);
    text_layer_set_text(s_round_layer, s_round_buf);
  } else {
    text_layer_set_text(s_round_layer, "");
  }
}

static uint16_t session_average(void) {
  if (s_session_count == 0) return 0;
  uint32_t sum = 0;
  for (int i = 0; i < s_session_count; i++) {
    sum += s_session_times[i];
  }
  return (uint16_t)(sum / s_session_count);
}

static uint16_t session_best(void) {
  if (s_session_count == 0) return 9999;
  uint16_t best = 9999;
  for (int i = 0; i < s_session_count; i++) {
    if (s_session_times[i] < best) best = s_session_times[i];
  }
  return best;
}

static void persist_best(void) {
  persist_write_int(PERSIST_KEY_BEST, (int32_t)s_all_time_best_ms);
}

static void load_best(void) {
  if (persist_exists(PERSIST_KEY_BEST)) {
    s_all_time_best_ms = (uint16_t)persist_read_int(PERSIST_KEY_BEST);
    s_best_ms = s_all_time_best_ms;
  }
}

// ---------- expanding ring ----------

static void ring_layer_update(Layer *layer, GContext *ctx) {
  if (!s_ring_active) return;
  GRect bounds = layer_get_bounds(layer);
  GPoint center = GPoint(bounds.size.w / 2, bounds.size.h / 2);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_circle(ctx, center, s_ring_radius);
}

static void ring_timer_callback(void *data) {
  s_ring_timer = NULL;
  if (!s_ring_active || s_state != STATE_READY) {
    s_ring_active = false;
    layer_mark_dirty(s_ring_layer);
    return;
  }
  s_ring_radius += RING_STEP_PX;
  // Stop when ring exceeds screen diagonal
  if (s_ring_radius > 200) {
    s_ring_active = false;
    layer_mark_dirty(s_ring_layer);
    return;
  }
  layer_mark_dirty(s_ring_layer);
  s_ring_timer = app_timer_register(RING_INTERVAL_MS, ring_timer_callback, NULL);
}

static void start_ring_animation(void) {
  s_ring_active = true;
  s_ring_radius = 0;
  layer_mark_dirty(s_ring_layer);
  s_ring_timer = app_timer_register(RING_INTERVAL_MS, ring_timer_callback, NULL);
}

static void stop_ring_animation(void) {
  s_ring_active = false;
  if (s_ring_timer) {
    app_timer_cancel(s_ring_timer);
    s_ring_timer = NULL;
  }
  layer_mark_dirty(s_ring_layer);
}

// ---------- game states ----------

static void show_idle(void) {
  s_state = STATE_IDLE;
  stop_ring_animation();
  set_bg(GColorBlack);
  text_layer_set_text(s_main_layer, "REACTION\nTESTER");
  text_layer_set_text_color(s_main_layer, GColorWhite);
  text_layer_set_text(s_result_layer, "");
  text_layer_set_text(s_best_layer, "");
  text_layer_set_text(s_hint_layer, "Press Select to start");
  text_layer_set_text(s_round_layer, "");
}

static void show_summary(void) {
  s_state = STATE_SUMMARY;
  stop_ring_animation();
  set_bg(PBL_IF_COLOR_ELSE(GColorDukeBlue, GColorBlack));

  text_layer_set_text(s_main_layer, "Session\nComplete!");
  text_layer_set_text_color(s_main_layer, GColorWhite);

  uint16_t avg = session_average();
  uint16_t best = session_best();
  snprintf(s_stats_buf, sizeof(s_stats_buf), "Avg: %d ms  Best: %d ms", avg, best);
  text_layer_set_text(s_result_layer, s_stats_buf);

  if (s_all_time_best_ms < 9999) {
    snprintf(s_best_buf, sizeof(s_best_buf), "All-time best: %d ms", s_all_time_best_ms);
  } else {
    s_best_buf[0] = '\0';
  }
  text_layer_set_text(s_best_layer, s_best_buf);
  text_layer_set_text(s_hint_layer, "Select: new session");
  text_layer_set_text(s_round_layer, "");
}

static void show_stats(void) {
  s_state = STATE_STATS;
  stop_ring_animation();
  set_bg(PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack));

  text_layer_set_text(s_main_layer, "Stats");
  text_layer_set_text_color(s_main_layer, GColorWhite);

  if (s_session_count > 0) {
    uint16_t avg = session_average();
    uint16_t best = session_best();
    snprintf(s_stats_buf, sizeof(s_stats_buf), "Last %d: avg %dms best %dms",
             s_session_count, avg, best);
  } else {
    snprintf(s_stats_buf, sizeof(s_stats_buf), "No rounds yet");
  }
  text_layer_set_text(s_result_layer, s_stats_buf);

  if (s_all_time_best_ms < 9999) {
    snprintf(s_best_buf, sizeof(s_best_buf), "All-time best: %d ms", s_all_time_best_ms);
  } else {
    snprintf(s_best_buf, sizeof(s_best_buf), "No all-time best");
  }
  text_layer_set_text(s_best_layer, s_best_buf);
  text_layer_set_text(s_hint_layer, "Down: back");
  text_layer_set_text(s_round_layer, "");
}

static void show_overlay(void) {
  s_state = STATE_OVERLAY;
  set_bg(PBL_IF_COLOR_ELSE(GColorDukeBlue, GColorBlack));
  text_layer_set_text(s_main_layer, "Reaction\nTester");
  text_layer_set_text_color(s_main_layer, GColorWhite);
  text_layer_set_text(s_result_layer, "Test your reflexes!");
  text_layer_set_text(s_best_layer, "Select: GO!  Up: Stats");
  text_layer_set_text(s_hint_layer, "Press any button");
  text_layer_set_text(s_round_layer, "");
}

static void trigger_fire(void *data) {
  s_delay_timer = NULL;
  s_state = STATE_READY;

  // Flash screen white and vibrate
  set_bg(GColorWhite);
  text_layer_set_text(s_main_layer, "NOW!");
  text_layer_set_text_color(s_main_layer, GColorBlack);
  text_layer_set_text(s_hint_layer, "");

  vibes_short_pulse();

  // Store ms timestamp
  time_t sec;
  uint16_t ms;
  time_ms(&sec, &ms);
  s_trigger_time = (uint16_t)((sec % 60) * 1000 + ms);

  // Start expanding ring
  start_ring_animation();

  update_round_display();
}

static void start_round(void) {
  // Start a new session if needed
  if (s_current_round == 0 || s_current_round >= s_max_rounds) {
    s_current_round = 0;
    s_session_count = 0;
  }

  s_current_round++;
  s_state = STATE_WAITING;
  stop_ring_animation();
  set_bg(PBL_IF_COLOR_ELSE(GColorDarkCandyAppleRed, GColorBlack));
  text_layer_set_text(s_main_layer, "Wait...");
  text_layer_set_text_color(s_main_layer, GColorWhite);
  text_layer_set_text(s_result_layer, "");
  text_layer_set_text(s_hint_layer, "");

  update_round_display();

  // Random delay 1000-5000ms
  int delay = 1000 + (rand() % 4000);
  s_delay_timer = app_timer_register(delay, trigger_fire, NULL);
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  switch (s_state) {
    case STATE_OVERLAY:
      show_idle();
      break;

    case STATE_IDLE:
    case STATE_TOO_EARLY:
      start_round();
      break;

    case STATE_RESULT:
      if (s_current_round >= s_max_rounds) {
        show_summary();
      } else {
        start_round();
      }
      break;

    case STATE_SUMMARY:
    case STATE_STATS:
      // New session from summary/stats
      s_current_round = 0;
      s_session_count = 0;
      start_round();
      break;

    case STATE_WAITING:
      // Too early!
      if (s_delay_timer) {
        app_timer_cancel(s_delay_timer);
        s_delay_timer = NULL;
      }
      s_state = STATE_TOO_EARLY;
      // Don't count this round - revert round counter
      s_current_round--;
      set_bg(PBL_IF_COLOR_ELSE(GColorOrange, GColorBlack));
      text_layer_set_text(s_main_layer, "TOO\nEARLY!");
      text_layer_set_text_color(s_main_layer, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
      text_layer_set_text(s_hint_layer, "Select to retry");
      text_layer_set_text(s_round_layer, "");

      static const uint32_t segs[] = {100, 50, 100, 50, 100};
      VibePattern pat = { .durations = segs, .num_segments = 5 };
      vibes_enqueue_custom_pattern(pat);
      break;

    case STATE_READY: {
      // Calculate reaction time
      stop_ring_animation();

      time_t sec;
      uint16_t ms;
      time_ms(&sec, &ms);
      uint16_t now = (uint16_t)((sec % 60) * 1000 + ms);

      if (now >= s_trigger_time) {
        s_reaction_ms = now - s_trigger_time;
      } else {
        s_reaction_ms = (60000 - s_trigger_time) + now;
      }

      s_state = STATE_RESULT;

      // Record in session
      if (s_session_count < s_max_rounds) {
        s_session_times[s_session_count] = s_reaction_ms;
        s_session_count++;
      }

      // Update best
      if (s_reaction_ms < s_best_ms) {
        s_best_ms = s_reaction_ms;
      }
      if (s_reaction_ms < s_all_time_best_ms) {
        s_all_time_best_ms = s_reaction_ms;
        persist_best();
      }

      set_bg(PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorBlack));
      snprintf(s_result_buf, sizeof(s_result_buf), "%d ms", s_reaction_ms);
      text_layer_set_text(s_main_layer, s_result_buf);
      text_layer_set_text_color(s_main_layer, GColorWhite);

      snprintf(s_best_buf, sizeof(s_best_buf), "Best: %d ms", s_best_ms);
      text_layer_set_text(s_best_layer, s_best_buf);

      // Rating
      const char *rating;
      if (s_reaction_ms < 200) rating = "Lightning!";
      else if (s_reaction_ms < 300) rating = "Fast!";
      else if (s_reaction_ms < 400) rating = "Good";
      else if (s_reaction_ms < 500) rating = "Average";
      else rating = "Slow...";
      text_layer_set_text(s_hint_layer, rating);

      // Round indicator
      if (s_current_round >= s_max_rounds) {
        snprintf(s_round_buf, sizeof(s_round_buf), "Round %d/%d DONE", s_current_round, s_max_rounds);
      } else {
        snprintf(s_round_buf, sizeof(s_round_buf), "Round %d/%d", s_current_round, s_max_rounds);
      }
      text_layer_set_text(s_round_layer, s_round_buf);

      break;
    }
  }
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  if (s_state == STATE_OVERLAY) {
    show_idle();
    return;
  }
  if (s_state == STATE_IDLE || s_state == STATE_STATS) {
    show_stats();
  }
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  if (s_state == STATE_OVERLAY) {
    show_idle();
    return;
  }
  if (s_state == STATE_STATS || s_state == STATE_SUMMARY) {
    show_idle();
    return;
  }
  // Reset best
  s_best_ms = 9999;
  s_all_time_best_ms = 9999;
  persist_best();
  s_current_round = 0;
  s_session_count = 0;
  text_layer_set_text(s_best_layer, "");
  show_idle();
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

// ---------- BT handler ----------

static void bt_handler(bool connected) {
  s_bt_connected = connected;
  if (!connected) {
    vibes_double_pulse();
    // Brief flash - show alert text
    text_layer_set_text(s_result_layer, "BT Disconnected!");
  }
}

// ---------- window load/unload ----------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  // Status bar with dotted separator
  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, GColorClear, GColorWhite);
  status_bar_layer_set_separator_mode(s_status_bar, StatusBarLayerSeparatorModeDotted);
  layer_add_child(root, status_bar_layer_get_layer(s_status_bar));

  int y_off = STATUS_BAR_HEIGHT;

  // Round indicator (top, just below status bar)
  s_round_layer = text_layer_create(GRect(0, y_off, bounds.size.w, 18));
  text_layer_set_background_color(s_round_layer, GColorClear);
  text_layer_set_text_color(s_round_layer, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  text_layer_set_font(s_round_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_round_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_round_layer));

  // Main text (center, shifted down by status bar height)
  s_main_layer = text_layer_create(GRect(0, y_off + 24, bounds.size.w, 80));
  text_layer_set_background_color(s_main_layer, GColorClear);
  text_layer_set_text_color(s_main_layer, GColorWhite);
  text_layer_set_font(s_main_layer, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
  text_layer_set_text_alignment(s_main_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_main_layer));

  // Result sub-text
  s_result_layer = text_layer_create(GRect(0, y_off + 88, bounds.size.w, 24));
  text_layer_set_background_color(s_result_layer, GColorClear);
  text_layer_set_text_color(s_result_layer, GColorWhite);
  text_layer_set_font(s_result_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_result_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_result_layer));

  // Best time
  s_best_layer = text_layer_create(GRect(0, y_off + 108, bounds.size.w, 20));
  text_layer_set_background_color(s_best_layer, GColorClear);
  text_layer_set_text_color(s_best_layer, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  text_layer_set_font(s_best_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_best_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_best_layer));

  // Hint at bottom
  s_hint_layer = text_layer_create(GRect(0, bounds.size.h - 24, bounds.size.w, 24));
  text_layer_set_background_color(s_hint_layer, GColorClear);
  text_layer_set_text_color(s_hint_layer, GColorWhite);
  text_layer_set_font(s_hint_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_hint_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_hint_layer));

  // Expanding ring overlay (full screen)
  s_ring_layer = layer_create(bounds);
  layer_set_update_proc(s_ring_layer, ring_layer_update);
  layer_add_child(root, s_ring_layer);

  // Show first-launch overlay or idle
  if (s_first_launch) {
    show_overlay();
  } else {
    show_idle();
  }
}

static void window_unload(Window *window) {
  text_layer_destroy(s_main_layer);
  text_layer_destroy(s_result_layer);
  text_layer_destroy(s_best_layer);
  text_layer_destroy(s_hint_layer);
  text_layer_destroy(s_round_layer);
  status_bar_layer_destroy(s_status_bar);
  layer_destroy(s_ring_layer);
}

// ---------- AppMessage handlers ----------

static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *rounds_t = dict_find(iter, MSG_KEY_ROUNDS);
  if (rounds_t) {
    int val = (int)rounds_t->value->int32;
    if (val == 5 || val == 10 || val == 15 || val == 20) {
      s_max_rounds = val;
      persist_write_int(PERSIST_KEY_ROUNDS, s_max_rounds);
    }
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", reason);
}

// ---------- init/deinit ----------

static void init(void) {
  srand(time(NULL));

  // Load persisted best
  load_best();

  // Load persisted rounds setting
  if (persist_exists(PERSIST_KEY_ROUNDS)) {
    s_max_rounds = (int)persist_read_int(PERSIST_KEY_ROUNDS);
    if (s_max_rounds < 5 || s_max_rounds > MAX_ROUNDS_LIMIT) {
      s_max_rounds = 10;
    }
  }

  // Check first launch
  if (!persist_exists(PERSIST_KEY_LAUNCHED)) {
    s_first_launch = true;
    persist_write_bool(PERSIST_KEY_LAUNCHED, true);
  }

  // BT monitoring
  s_bt_connected = connection_service_peek_pebble_app_connection();
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bt_handler,
  });

  // AppMessage for phone configuration
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
  if (s_delay_timer) app_timer_cancel(s_delay_timer);
  if (s_ring_timer) app_timer_cancel(s_ring_timer);
  connection_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
