#include <pebble.h>

static Window *s_window;
static TextLayer *s_time_layer;
static TextLayer *s_phase_layer;
static TextLayer *s_count_layer;
static Layer *s_progress_layer;

typedef enum {
  PHASE_WORK,
  PHASE_SHORT_BREAK,
  PHASE_LONG_BREAK,
} Phase;

#define WORK_SECONDS     (25 * 60)
#define SHORT_BREAK_SEC  (5 * 60)
#define LONG_BREAK_SEC   (15 * 60)
#define POMOS_BEFORE_LONG 4

static Phase s_phase = PHASE_WORK;
static int s_seconds_left = WORK_SECONDS;
static int s_phase_duration = WORK_SECONDS;
static int s_pomo_count = 0;
static bool s_running = false;
static char s_time_buf[8];
static char s_count_buf[16];

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

static void progress_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int bar_h = 8;
  int y = bounds.size.h - bar_h - 2;
  int margin = 10;
  int max_w = bounds.size.w - 2 * margin;

  // Background bar
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
  graphics_fill_rect(ctx, GRect(margin, y, max_w, bar_h), 2, GCornersAll);

  // Progress fill
  float pct = 1.0f - ((float)s_seconds_left / (float)s_phase_duration);
  int fill_w = (int)(pct * max_w);
  if (fill_w > 0) {
    GColor fill_color;
    if (s_phase == PHASE_WORK) {
      fill_color = PBL_IF_COLOR_ELSE(GColorRed, GColorWhite);
    } else {
      fill_color = PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite);
    }
    graphics_context_set_fill_color(ctx, fill_color);
    graphics_fill_rect(ctx, GRect(margin, y, fill_w, bar_h), 2, GCornersAll);
  }
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

static void select_click(ClickRecognizerRef recognizer, void *context) {
  s_running = !s_running;
  update_display();
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  // Reset current phase
  s_seconds_left = s_phase_duration;
  s_running = false;
  update_display();
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  // Skip to next phase
  next_phase();
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

  // Phase label
  s_phase_layer = text_layer_create(GRect(0, 20, bounds.size.w, 30));
  text_layer_set_background_color(s_phase_layer, GColorClear);
  text_layer_set_text_color(s_phase_layer, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
  text_layer_set_font(s_phase_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_phase_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_phase_layer));

  // Time display
  s_time_layer = text_layer_create(GRect(0, 50, bounds.size.w, 60));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  // Pomo count
  s_count_layer = text_layer_create(GRect(0, 115, bounds.size.w, 24));
  text_layer_set_background_color(s_count_layer, GColorClear);
  text_layer_set_text_color(s_count_layer, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  text_layer_set_font(s_count_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_count_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_count_layer));

  // Progress bar layer
  s_progress_layer = layer_create(bounds);
  layer_set_update_proc(s_progress_layer, progress_update);
  layer_add_child(root, s_progress_layer);

  update_display();
}

static void window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_phase_layer);
  text_layer_destroy(s_count_layer);
  layer_destroy(s_progress_layer);
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
