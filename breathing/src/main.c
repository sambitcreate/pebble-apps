#include <pebble.h>

static Window *s_window;
static Layer *s_canvas;
static TextLayer *s_phase_layer;
static TextLayer *s_pattern_layer;
static TextLayer *s_count_layer;

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

static const Pattern PATTERNS[] = {
  {"4-7-8",   4, 7, 8, 0},
  {"Box",     4, 4, 4, 4},
  {"Simple",  4, 0, 4, 0},
};
#define NUM_PATTERNS 3

static int s_pattern_idx = 0;
static BreathPhase s_phase = BREATH_INHALE;
static int s_phase_elapsed = 0;   // tenths of seconds into current phase
static int s_phase_duration = 0;  // tenths of seconds
static int s_cycles = 0;
static bool s_running = false;
static int s_circle_radius = 10;
static char s_count_buf[16];

#define MIN_RADIUS 10
#define MAX_RADIUS 55
#define TICK_MS 100

static AppTimer *s_tick_timer;

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

  // Vibrate at transition
  vibes_short_pulse();
}

static void next_phase(void) {
  BreathPhase next = (s_phase + 1) % 4;
  if (next == BREATH_INHALE) {
    s_cycles++;
  }
  start_phase(next);
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int cx = bounds.size.w / 2;
  int cy = bounds.size.h / 2 - 5;

  // Outer ring
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, GPoint(cx, cy), MAX_RADIUS + 2);

  // Breathing circle
  GColor fill;
  switch (s_phase) {
    case BREATH_INHALE:
      fill = PBL_IF_COLOR_ELSE(GColorCyan, GColorWhite);
      break;
    case BREATH_HOLD:
    case BREATH_HOLD2:
      fill = PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite);
      break;
    case BREATH_EXHALE:
      fill = PBL_IF_COLOR_ELSE(GColorMagenta, GColorWhite);
      break;
    default:
      fill = GColorWhite;
  }

  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_circle(ctx, GPoint(cx, cy), s_circle_radius);

  // Cycle count
  snprintf(s_count_buf, sizeof(s_count_buf), "Breaths: %d", s_cycles);
  text_layer_set_text(s_count_layer, s_count_buf);
}

static void tick_callback(void *data) {
  if (!s_running) return;

  s_phase_elapsed++;

  // Calculate circle radius based on phase
  float progress = (float)s_phase_elapsed / (float)s_phase_duration;
  if (progress > 1.0f) progress = 1.0f;

  switch (s_phase) {
    case BREATH_INHALE:
      s_circle_radius = MIN_RADIUS + (int)((MAX_RADIUS - MIN_RADIUS) * progress);
      break;
    case BREATH_HOLD:
    case BREATH_HOLD2:
      // Stay at current size
      if (s_phase == BREATH_HOLD) {
        s_circle_radius = MAX_RADIUS;
      } else {
        s_circle_radius = MIN_RADIUS;
      }
      break;
    case BREATH_EXHALE:
      s_circle_radius = MAX_RADIUS - (int)((MAX_RADIUS - MIN_RADIUS) * progress);
      break;
  }

  layer_mark_dirty(s_canvas);

  if (s_phase_elapsed >= s_phase_duration) {
    next_phase();
  }

  s_tick_timer = app_timer_register(TICK_MS, tick_callback, NULL);
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  s_running = !s_running;
  if (s_running) {
    s_circle_radius = MIN_RADIUS;
    start_phase(BREATH_INHALE);
    s_tick_timer = app_timer_register(TICK_MS, tick_callback, NULL);
  } else {
    text_layer_set_text(s_phase_layer, "Paused");
    if (s_tick_timer) {
      app_timer_cancel(s_tick_timer);
      s_tick_timer = NULL;
    }
  }
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  s_pattern_idx = (s_pattern_idx + 1) % NUM_PATTERNS;
  text_layer_set_text(s_pattern_layer, PATTERNS[s_pattern_idx].name);
  if (s_running) {
    start_phase(BREATH_INHALE);
  }
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  s_pattern_idx = (s_pattern_idx + NUM_PATTERNS - 1) % NUM_PATTERNS;
  text_layer_set_text(s_pattern_layer, PATTERNS[s_pattern_idx].name);
  if (s_running) {
    start_phase(BREATH_INHALE);
  }
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

  // Pattern name at top
  s_pattern_layer = text_layer_create(GRect(0, 2, bounds.size.w, 20));
  text_layer_set_background_color(s_pattern_layer, GColorClear);
  text_layer_set_text_color(s_pattern_layer, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  text_layer_set_font(s_pattern_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_pattern_layer, GTextAlignmentCenter);
  text_layer_set_text(s_pattern_layer, PATTERNS[s_pattern_idx].name);
  layer_add_child(root, text_layer_get_layer(s_pattern_layer));

  // Canvas for circle
  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  // Phase text
  s_phase_layer = text_layer_create(GRect(0, bounds.size.h - 48, bounds.size.w, 24));
  text_layer_set_background_color(s_phase_layer, GColorClear);
  text_layer_set_text_color(s_phase_layer, GColorWhite);
  text_layer_set_font(s_phase_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_phase_layer, GTextAlignmentCenter);
  text_layer_set_text(s_phase_layer, "Press Select");
  layer_add_child(root, text_layer_get_layer(s_phase_layer));

  // Cycle count
  s_count_layer = text_layer_create(GRect(0, bounds.size.h - 24, bounds.size.w, 24));
  text_layer_set_background_color(s_count_layer, GColorClear);
  text_layer_set_text_color(s_count_layer, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
  text_layer_set_font(s_count_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_count_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_count_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(s_phase_layer);
  text_layer_destroy(s_pattern_layer);
  text_layer_destroy(s_count_layer);
  layer_destroy(s_canvas);
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
  if (s_tick_timer) app_timer_cancel(s_tick_timer);
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
