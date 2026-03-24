#include <pebble.h>

static Window *s_window;
static TextLayer *s_main_layer;
static TextLayer *s_result_layer;
static TextLayer *s_best_layer;
static TextLayer *s_hint_layer;
static AppTimer *s_delay_timer;

typedef enum {
  STATE_IDLE,
  STATE_WAITING,
  STATE_READY,
  STATE_RESULT,
  STATE_TOO_EARLY,
} GameState;

static GameState s_state = STATE_IDLE;
static uint16_t s_trigger_time;
static uint16_t s_reaction_ms;
static uint16_t s_best_ms = 9999;
static char s_result_buf[32];
static char s_best_buf[32];

static void set_bg(GColor color) {
  window_set_background_color(s_window, color);
}

static void show_idle(void) {
  s_state = STATE_IDLE;
  set_bg(GColorBlack);
  text_layer_set_text(s_main_layer, "REACTION\nTESTER");
  text_layer_set_text_color(s_main_layer, GColorWhite);
  text_layer_set_text(s_result_layer, "");
  text_layer_set_text(s_hint_layer, "Press Select to start");
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

  s_trigger_time = time_ms(NULL, &s_trigger_time);
  // Store full ms timestamp
  time_t sec;
  uint16_t ms;
  time_ms(&sec, &ms);
  s_trigger_time = (uint16_t)((sec % 60) * 1000 + ms);
}

static void start_round(void) {
  s_state = STATE_WAITING;
  set_bg(PBL_IF_COLOR_ELSE(GColorDarkCandyAppleRed, GColorBlack));
  text_layer_set_text(s_main_layer, "Wait...");
  text_layer_set_text_color(s_main_layer, GColorWhite);
  text_layer_set_text(s_result_layer, "");
  text_layer_set_text(s_hint_layer, "");

  // Random delay 1000-5000ms
  int delay = 1000 + (rand() % 4000);
  s_delay_timer = app_timer_register(delay, trigger_fire, NULL);
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  switch (s_state) {
    case STATE_IDLE:
    case STATE_RESULT:
    case STATE_TOO_EARLY:
      start_round();
      break;

    case STATE_WAITING:
      // Too early!
      if (s_delay_timer) {
        app_timer_cancel(s_delay_timer);
        s_delay_timer = NULL;
      }
      s_state = STATE_TOO_EARLY;
      set_bg(PBL_IF_COLOR_ELSE(GColorOrange, GColorBlack));
      text_layer_set_text(s_main_layer, "TOO\nEARLY!");
      text_layer_set_text_color(s_main_layer, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
      text_layer_set_text(s_hint_layer, "Select to retry");

      static const uint32_t segs[] = {100, 50, 100, 50, 100};
      VibePattern pat = { .durations = segs, .num_segments = 5 };
      vibes_enqueue_custom_pattern(pat);
      break;

    case STATE_READY: {
      // Calculate reaction time
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

      // Update best
      if (s_reaction_ms < s_best_ms) {
        s_best_ms = s_reaction_ms;
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

      break;
    }
  }
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  // Show best
  if (s_best_ms < 9999) {
    snprintf(s_best_buf, sizeof(s_best_buf), "Best: %d ms", s_best_ms);
  } else {
    snprintf(s_best_buf, sizeof(s_best_buf), "No best yet");
  }
  text_layer_set_text(s_result_layer, s_best_buf);
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  // Reset best
  s_best_ms = 9999;
  text_layer_set_text(s_best_layer, "");
  show_idle();
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  // Main text (center)
  s_main_layer = text_layer_create(GRect(0, 40, bounds.size.w, 80));
  text_layer_set_background_color(s_main_layer, GColorClear);
  text_layer_set_text_color(s_main_layer, GColorWhite);
  text_layer_set_font(s_main_layer, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
  text_layer_set_text_alignment(s_main_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_main_layer));

  // Result sub-text
  s_result_layer = text_layer_create(GRect(0, 100, bounds.size.w, 24));
  text_layer_set_background_color(s_result_layer, GColorClear);
  text_layer_set_text_color(s_result_layer, GColorWhite);
  text_layer_set_font(s_result_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_result_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_result_layer));

  // Best time
  s_best_layer = text_layer_create(GRect(0, 120, bounds.size.w, 20));
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

  show_idle();
}

static void window_unload(Window *window) {
  text_layer_destroy(s_main_layer);
  text_layer_destroy(s_result_layer);
  text_layer_destroy(s_best_layer);
  text_layer_destroy(s_hint_layer);
}

static void init(void) {
  srand(time(NULL));
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
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
