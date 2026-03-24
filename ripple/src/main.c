#include <pebble.h>

static Window *s_window;
static Layer *s_canvas_layer;
static TextLayer *s_time_layer;

static AppTimer *s_pulse_timer;

static int s_hour_ring_radius;
static int s_minute_ring_radius;
static int s_pulse_radius;

// Pulse animation state
static int s_pulse_step;          // 0..20 (50ms * 20 = 1000ms = 1 second)
#define PULSE_STEPS 20
#define PULSE_MAX_RADIUS 65
#define PULSE_INTERVAL_MS 50

static char s_time_buf[8];

// ---- Drawing helpers ----

static void draw_circle_outline(GContext *ctx, GPoint center, int radius, int stroke_width) {
  if (radius <= 0) return;
  graphics_context_set_stroke_width(ctx, stroke_width);
  graphics_draw_circle(ctx, center, radius);
}

static void draw_crosshair(GContext *ctx, GPoint center) {
  // 4 short lines, each 4px long, from center
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_line(ctx, GPoint(center.x - 4, center.y), GPoint(center.x - 1, center.y));
  graphics_draw_line(ctx, GPoint(center.x + 1, center.y), GPoint(center.x + 4, center.y));
  graphics_draw_line(ctx, GPoint(center.x, center.y - 4), GPoint(center.x, center.y - 1));
  graphics_draw_line(ctx, GPoint(center.x, center.y + 1), GPoint(center.x, center.y + 4));
}

// ---- Canvas update ----

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);

  // Dark background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Ring 1 - Hours
#if defined(PBL_COLOR)
  graphics_context_set_stroke_color(ctx, GColorCyan);
#else
  graphics_context_set_stroke_color(ctx, GColorWhite);
#endif
  draw_circle_outline(ctx, center, s_hour_ring_radius, 2);

  // Ring 2 - Minutes
#if defined(PBL_COLOR)
  graphics_context_set_stroke_color(ctx, GColorMagenta);
#else
  graphics_context_set_stroke_color(ctx, GColorWhite);
#endif
  draw_circle_outline(ctx, center, s_minute_ring_radius, 1);

  // Ring 3 - Pulse (seconds)
#if defined(PBL_COLOR)
  graphics_context_set_stroke_color(ctx, GColorYellow);
#else
  graphics_context_set_stroke_color(ctx, GColorLightGray);
#endif
  draw_circle_outline(ctx, center, s_pulse_radius, 1);

  // Crosshair at center
  draw_crosshair(ctx, center);
}

// ---- Radius mapping ----

// Map a value in [0, max_val) to [10, 60] px
static int map_radius(int value, int max_val) {
  if (max_val <= 0) return 10;
  return 10 + (value * 50) / (max_val - 1 > 0 ? max_val - 1 : 1);
}

// ---- Pulse timer callback ----

static void pulse_timer_callback(void *data) {
  s_pulse_step++;
  if (s_pulse_step >= PULSE_STEPS) {
    s_pulse_step = 0;
  }

  // Linear expansion from 0 to PULSE_MAX_RADIUS over one second
  s_pulse_radius = (PULSE_MAX_RADIUS * s_pulse_step) / PULSE_STEPS;

  // Mark canvas dirty to redraw
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }

  // Re-register timer
  s_pulse_timer = app_timer_register(PULSE_INTERVAL_MS, pulse_timer_callback, NULL);
}

// ---- Tick handler ----

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // Update hour ring: use 12-hour value
  int hour12 = tick_time->tm_hour % 12;
  s_hour_ring_radius = map_radius(hour12, 12);

  // Update minute ring
  s_minute_ring_radius = map_radius(tick_time->tm_min, 60);

  // Reset pulse step at each new second so it stays in sync
  s_pulse_step = 0;
  s_pulse_radius = 0;

  // Update time text
  if (clock_is_24h_style()) {
    strftime(s_time_buf, sizeof(s_time_buf), "%H:%M", tick_time);
  } else {
    strftime(s_time_buf, sizeof(s_time_buf), "%I:%M", tick_time);
  }
  text_layer_set_text(s_time_layer, s_time_buf);

  // Mark dirty (pulse timer also marks dirty, but ensure immediate update)
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

// ---- Window handlers ----

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Canvas layer fills entire window
  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  // Time text layer at the very bottom
  GRect time_frame = GRect(0, bounds.size.h - 20, bounds.size.w, 20);
  s_time_layer = text_layer_create(time_frame);
  text_layer_set_background_color(s_time_layer, GColorClear);
#if defined(PBL_COLOR)
  text_layer_set_text_color(s_time_layer, GColorDarkGray);
#else
  text_layer_set_text_color(s_time_layer, GColorLightGray);
#endif
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  text_layer_set_text(s_time_layer, "00:00");
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  // Initialize with current time
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  tick_handler(t, SECOND_UNIT);

  // Start pulse animation timer
  s_pulse_step = 0;
  s_pulse_radius = 0;
  s_pulse_timer = app_timer_register(PULSE_INTERVAL_MS, pulse_timer_callback, NULL);
}

static void window_unload(Window *window) {
  // Cancel pulse timer
  if (s_pulse_timer) {
    app_timer_cancel(s_pulse_timer);
    s_pulse_timer = NULL;
  }

  text_layer_destroy(s_time_layer);
  layer_destroy(s_canvas_layer);
}

// ---- App lifecycle ----

static void init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  // Subscribe to second ticks for ring 1 and ring 2 updates
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
