#include <pebble.h>

#define STORAGE_KEY_COUNT 1

static Window *s_window;
static TextLayer *s_count_layer;
static TextLayer *s_label_layer;
static TextLayer *s_hint_layer;
static Layer *s_bar_layer;

static int s_count = 0;
static char s_count_buf[12];

static void save_count(void) {
  persist_write_int(STORAGE_KEY_COUNT, s_count);
}

static void load_count(void) {
  if (persist_exists(STORAGE_KEY_COUNT)) {
    s_count = persist_read_int(STORAGE_KEY_COUNT);
  } else {
    s_count = 0;
  }
}

static void update_display(void) {
  snprintf(s_count_buf, sizeof(s_count_buf), "%d", s_count);
  text_layer_set_text(s_count_layer, s_count_buf);
  layer_mark_dirty(s_bar_layer);
}

static void bar_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int bar_h = 4;
  int y = bounds.size.h - bar_h;

  // Background
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
  graphics_fill_rect(ctx, GRect(0, y, bounds.size.w, bar_h), 0, GCornerNone);

  // Fill based on count mod 100
  int pct = s_count % 100;
  int fill_w = (pct * bounds.size.w) / 100;
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
  graphics_fill_rect(ctx, GRect(0, y, fill_w, bar_h), 0, GCornerNone);
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  s_count++;
  update_display();
  save_count();

  // Subtle haptic
  static const uint32_t segs[] = {30};
  VibePattern pat = { .durations = segs, .num_segments = 1 };
  vibes_enqueue_custom_pattern(pat);
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  s_count += 5;
  update_display();
  save_count();
  vibes_short_pulse();
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  if (s_count > 0) {
    s_count--;
    update_display();
    save_count();
  }
}

static void select_long_click(ClickRecognizerRef recognizer, void *context) {
  s_count = 0;
  update_display();
  save_count();

  // Double vibrate for reset
  static const uint32_t segs[] = {100, 80, 100};
  VibePattern pat = { .durations = segs, .num_segments = 3 };
  vibes_enqueue_custom_pattern(pat);
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 1000, select_long_click, NULL);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, GColorBlack);

  // Label
  s_label_layer = text_layer_create(GRect(0, 15, bounds.size.w, 24));
  text_layer_set_background_color(s_label_layer, GColorClear);
  text_layer_set_text_color(s_label_layer, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
  text_layer_set_font(s_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_label_layer, GTextAlignmentCenter);
  text_layer_set_text(s_label_layer, "TALLY");
  layer_add_child(root, text_layer_get_layer(s_label_layer));

  // Big count
  s_count_layer = text_layer_create(GRect(0, 40, bounds.size.w, 70));
  text_layer_set_background_color(s_count_layer, GColorClear);
  text_layer_set_text_color(s_count_layer, GColorWhite);
  text_layer_set_font(s_count_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_count_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_count_layer));

  // Hints
  s_hint_layer = text_layer_create(GRect(0, 120, bounds.size.w, 40));
  text_layer_set_background_color(s_hint_layer, GColorClear);
  text_layer_set_text_color(s_hint_layer, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
  text_layer_set_font(s_hint_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_hint_layer, GTextAlignmentCenter);
  text_layer_set_text(s_hint_layer, "Sel:+1  Up:+5  Down:-1\nHold Sel: Reset");
  layer_add_child(root, text_layer_get_layer(s_hint_layer));

  // Progress bar
  s_bar_layer = layer_create(bounds);
  layer_set_update_proc(s_bar_layer, bar_update);
  layer_add_child(root, s_bar_layer);

  load_count();
  update_display();
}

static void window_unload(Window *window) {
  text_layer_destroy(s_count_layer);
  text_layer_destroy(s_label_layer);
  text_layer_destroy(s_hint_layer);
  layer_destroy(s_bar_layer);
}

static void init(void) {
  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

static void deinit(void) {
  save_count();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
