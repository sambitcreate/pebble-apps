#include <pebble.h>

static Window *s_window;
static TextLayer *s_time_layer;
static TextLayer *s_morse_layer;
static TextLayer *s_hint_layer;

// Morse code for digits 0-9
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

// Morse string representations
static const char *MORSE_STR[10] = {
  "-----", ".----", "..---", "...--", "....-",
  ".....", "-....", "--...", "---..", "----.",
};

#define DOT_MS   100
#define DASH_MS  300
#define GAP_MS   100
#define CHAR_GAP 300
#define WORD_GAP 700

#define MAX_VIBE 64

static uint32_t s_vibe_segments[MAX_VIBE];
static int s_vibe_count;
static bool s_24h = true;
static char s_time_buf[8];
static char s_morse_buf[64];

static void update_display(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  int h = s_24h ? t->tm_hour : (t->tm_hour % 12 == 0 ? 12 : t->tm_hour % 12);
  int m = t->tm_min;

  snprintf(s_time_buf, sizeof(s_time_buf), "%02d:%02d", h, m);
  text_layer_set_text(s_time_layer, s_time_buf);

  // Build morse string
  int h1 = h / 10, h0 = h % 10;
  int m1 = m / 10, m0 = m % 10;
  snprintf(s_morse_buf, sizeof(s_morse_buf), "%s %s : %s %s",
           MORSE_STR[h1], MORSE_STR[h0],
           MORSE_STR[m1], MORSE_STR[m0]);
  text_layer_set_text(s_morse_layer, s_morse_buf);
}

static void build_vibe_for_digit(int digit) {
  for (int i = 0; MORSE_DIGITS[digit][i] != -1 && s_vibe_count < MAX_VIBE - 1; i++) {
    if (i > 0 && s_vibe_count < MAX_VIBE) {
      s_vibe_segments[s_vibe_count++] = GAP_MS;  // inter-element gap (silence)
    }
    s_vibe_segments[s_vibe_count++] = MORSE_DIGITS[digit][i] ? DASH_MS : DOT_MS;
  }
}

static void vibrate_time(bool minutes_only) {
  s_vibe_count = 0;

  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  int h = s_24h ? t->tm_hour : (t->tm_hour % 12 == 0 ? 12 : t->tm_hour % 12);
  int m = t->tm_min;

  int digits[4] = {h / 10, h % 10, m / 10, m % 10};
  int start = minutes_only ? 2 : 0;

  for (int d = start; d < 4; d++) {
    if (d > start && s_vibe_count < MAX_VIBE) {
      s_vibe_segments[s_vibe_count++] = CHAR_GAP;  // inter-character gap
    }
    if (d == 2 && !minutes_only && s_vibe_count < MAX_VIBE) {
      s_vibe_segments[s_vibe_count++] = WORD_GAP;  // hour:minute separator
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

static void select_click(ClickRecognizerRef recognizer, void *context) {
  vibrate_time(false);
  text_layer_set_text(s_hint_layer, "Vibrating time...");
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  s_24h = !s_24h;
  update_display();
  text_layer_set_text(s_hint_layer, s_24h ? "24h mode" : "12h mode");
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  vibrate_time(true);
  text_layer_set_text(s_hint_layer, "Vibrating minutes...");
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  update_display();
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, GColorBlack);

  // Digital time
  s_time_layer = text_layer_create(GRect(0, 25, bounds.size.w, 50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  // Morse code display
  s_morse_layer = text_layer_create(GRect(5, 80, bounds.size.w - 10, 50));
  text_layer_set_background_color(s_morse_layer, GColorClear);
  text_layer_set_text_color(s_morse_layer, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
  text_layer_set_font(s_morse_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_morse_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_morse_layer));

  // Hint
  s_hint_layer = text_layer_create(GRect(0, bounds.size.h - 24, bounds.size.w, 24));
  text_layer_set_background_color(s_hint_layer, GColorClear);
  text_layer_set_text_color(s_hint_layer, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
  text_layer_set_font(s_hint_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_hint_layer, GTextAlignmentCenter);
  text_layer_set_text(s_hint_layer, "Select: vibrate time");
  layer_add_child(root, text_layer_get_layer(s_hint_layer));

  update_display();
}

static void window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_morse_layer);
  text_layer_destroy(s_hint_layer);
}

static void init(void) {
  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
